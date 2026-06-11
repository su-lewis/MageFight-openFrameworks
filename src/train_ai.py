import gymnasium as gym
from gymnasium import spaces
import numpy as np
import zmq
import struct
from stable_baselines3 import PPO

class MageFightEnv(gym.Env):
    def __init__(self):
        super(MageFightEnv, self).__init__()
        
        # Action space: Let's assume up to 4000 possible actions 
        # (0-99=Play Card, 1000=Draft, 2000=Menu, 3000=Draw, 9999=EndTurn)
        # We will use a Discrete space of 10000 for simplicity right now.
        self.action_space = spaces.Discrete(20000)
        
        # Observation space: 5700 floats (from extractGameStateForAI)
        self.observation_space = spaces.Box(low=-1000.0, high=1000.0, shape=(5700,), dtype=np.float32)
        
        # Setup ZeroMQ Server
        context = zmq.Context()
        self.socket = context.socket(zmq.REP)
        self.socket.bind("tcp://*:5555")
        print("Python AI Server listening on port 5555...")

    def reset(self, seed=None, options=None):
        # In a real setup, you might tell C++ to reset. 
        # Here, C++ drives the loop, so we just wait for the first state.
        state, reward, done = self._receive_state_from_cpp()
        return state, {}

    def step(self, action):
        # Save the old state (HP, AP, Turn Counter) to see if the AI actually did something
        old_ap = self.last_state[1] if hasattr(self, 'last_state') else 0
        old_turn = self.last_state[0] if hasattr(self, 'last_state') else 0
        
        # 1. Send action to C++
        self.socket.send(struct.pack('i', int(action)))
        
        # 2. Wait for C++ to execute it, step the game, and send back the new state
        state, reward, done = self._receive_state_from_cpp()
        self.last_state = state
        
        # 3. Punish illegal/wasted moves!
        # If the turn counter and AP didn't change, the AI tried an invalid move and C++ ignored it.
        if not done and state[1] == old_ap and state[0] == old_turn:
            reward -= 0.05  # Tiny slap on the wrist for doing an illegal move
            
        # 4. Time penalty (Encourage it to win quickly)
        reward -= 0.001 
        
        truncated = False
        return state, reward, done, truncated, {}

    def _receive_state_from_cpp(self):
        # Block and wait for C++ to send a byte array
        message = self.socket.recv()
        
        # Convert bytes to numpy float32 array
        data = np.frombuffer(message, dtype=np.float32)
        
        done = bool(data[0])
        reward = float(data[1])
        
        # The rest is the 5700 float state tensor
        state = data[2:]
        
        # Pad with zeros if C++ sent slightly less than 5700, or trim if more
        padded_state = np.zeros(5700, dtype=np.float32)
        length = min(len(state), 5700)
        padded_state[:length] = state[:length]
        
        return padded_state, reward, done

# --- Training Loop ---
if __name__ == "__main__":
    # Create the environment
    env = MageFightEnv()
    
    # Initialize PPO (Proximal Policy Optimization) - the industry standard algorithm
    print("Initializing PPO Model...")
    model = PPO("MlpPolicy", env, verbose=1, tensorboard_log="./ppo_magefight_tensorboard/")
    
    # Train the model! (This will block, waiting for C++ to connect)
    print("Waiting for C++ Game to launch...")
    model.learn(total_timesteps=10000000)
    
    # Save the brain
    model.save("magefight_ai_v1")