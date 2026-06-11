import numpy as np
import zmq
import struct
import torch
import torch.nn as nn
import gymnasium as gym
from gymnasium import spaces
from sb3_contrib import MaskablePPO
from stable_baselines3.common.torch_layers import BaseFeaturesExtractor

# --- 1. Custom CNN for MageFight Grid ---
class MageFightFeatureExtractor(BaseFeaturesExtractor):
    def __init__(self, observation_space: gym.spaces.Box, features_dim: int = 512):
        super().__init__(observation_space, features_dim)
        
        # State layout from C++:
        # 5 (Global) + 16 (P0) + 16 (P1) + 70 (Hand) + 70 (Draft) = 177 Global Features
        # 15 * 11 * 33 = 5445 Board Features
        # Total = 5622 (C++ pads to 5700)
        self.global_dim = 177
        self.board_channels = 33
        self.board_width = 15
        self.board_height = 11
        
        # Spatial network
        self.cnn = nn.Sequential(
            nn.Conv2d(self.board_channels, 64, kernel_size=3, stride=1, padding=1),
            nn.ReLU(),
            nn.Conv2d(64, 128, kernel_size=3, stride=1, padding=1),
            nn.ReLU(),
            nn.Flatten(),
        )
        
        cnn_output_dim = 128 * self.board_height * self.board_width 
        
        # Merge network
        self.linear = nn.Sequential(
            nn.Linear(cnn_output_dim + self.global_dim, features_dim),
            nn.ReLU()
        )

    def forward(self, observations: torch.Tensor) -> torch.Tensor:
        global_features = observations[:, :self.global_dim]
        board_features = observations[:, self.global_dim : self.global_dim + (self.board_channels * self.board_width * self.board_height)]
        
        board_reshaped = board_features.view(-1, self.board_channels, self.board_height, self.board_width)
        cnn_out = self.cnn(board_reshaped)
        
        combined = torch.cat((cnn_out, global_features), dim=1)
        return self.linear(combined)

# --- 2. ZMQ Environment ---
class MageFightEnv(gym.Env):
    def __init__(self):
        super(MageFightEnv, self).__init__()
        
        self.action_space = spaces.Discrete(20000)
        self.observation_space = spaces.Box(low=-1000.0, high=1000.0, shape=(5700,), dtype=np.float32)
        
        context = zmq.Context()
        self.socket = context.socket(zmq.REP)
        self.socket.bind("tcp://*:5555")
        print("Python AI Server listening on port 5555...")
        
        self.current_mask = np.zeros(20000, dtype=np.int8)
        self.last_state = np.zeros(5700, dtype=np.float32)

    def action_masks(self):
        # MaskablePPO will call this automatically to filter invalid actions
        return self.current_mask

    def reset(self, seed=None, options=None):
        state, reward, done = self._receive_state_from_cpp()
        self.last_state = state
        return state, {}

    def step(self, action):
        old_ap = self.last_state[1]
        old_turn = self.last_state[0]
        
        # Send action to C++
        self.socket.send(struct.pack('i', int(action)))
        
        # Wait for simulation to run
        state, reward, done = self._receive_state_from_cpp()
        self.last_state = state
        
        # Punish stalling or illegal moves
        if not done and state[1] == old_ap and state[0] == old_turn:
            reward -= 0.05
            
        reward -= 0.001 # Time penalty
        
        return state, reward, done, False, {}

    def _receive_state_from_cpp(self):
        message = self.socket.recv()
        data = np.frombuffer(message, dtype=np.float32)
        
        done = bool(data[0])
        reward = float(data[1])
        state = data[2:5702]
        
        # Binary mask from C++ 
        # Float 0.0/1.0 converted to bool/int8 for sb3-contrib
        mask_floats = data[5702:25702]
        self.current_mask = (mask_floats > 0.5).astype(np.int8) 

        # Guarantee at least ONE valid action to prevent SB3 crashes
        if not np.any(self.current_mask):
            self.current_mask[0] = 1 # Fallback: End Turn is always valid
        
        padded_state = np.zeros(5700, dtype=np.float32)
        length = min(len(state), 5700)
        padded_state[:length] = state[:length]
        
        return padded_state, reward, done

# --- 3. Training Loop ---
if __name__ == "__main__":
    env = MageFightEnv()
    
    policy_kwargs = dict(
        features_extractor_class=MageFightFeatureExtractor,
        features_extractor_kwargs=dict(features_dim=512),
    )
    
    print("Initializing MaskablePPO Model...")
    model = MaskablePPO(
        "MlpPolicy", 
        env, 
        policy_kwargs=policy_kwargs, 
        verbose=1, 
        tensorboard_log="./magefight_tensorboard/"
    )
    
    print("Waiting for C++ Game to launch...")
    
    try:
        model.learn(total_timesteps=10_000_000)
    except KeyboardInterrupt:
        print("\nTraining interrupted! Saving model...")
    finally:
        model.save("magefight_ai_v1")
        print("Model saved to magefight_ai_v1.zip")