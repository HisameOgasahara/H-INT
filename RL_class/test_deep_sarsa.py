import random
import numpy as np
from environment_deep_sarsa import Env
import tensorflow as tf
from tensorflow.keras.layers import Dense


class DeepSARSA(tf.keras.Model):
    def __init__(self, action_size):
        super(DeepSARSA, self).__init__()
        self.fc1 = Dense(30, activation='relu')
        self.fc2 = Dense(30, activation='relu')
        self.fc_out = Dense(action_size)

    def call(self, x):
        x = self.fc1(x)
        x = self.fc2(x)
        q = self.fc_out(x)
        return q


class DeepSARSAgent:
    def __init__(self, state_size, action_size):
        self.state_size = state_size
        self.action_size = action_size
        self.epsilon = 0.01
        self.model = DeepSARSA(self.action_size)
        self.model(tf.zeros((1, self.state_size)))
        self.model.load_weights('save_model/model_deep_sarsa.weights.h5')

    def get_action(self, state):
        if np.random.rand() <= self.epsilon:
            return random.randrange(self.action_size)
        else:
            q_values = self.model(state)
            return np.argmax(q_values[0])


if __name__ == '__main__':
    env = Env(render_speed=0.05)
    state_size = 15
    action_space = [0, 1, 2, 3]
    action_size = len(action_space)
    agent = DeepSARSAgent(state_size, action_size)
    scores, episodes = [], []
    EPISODES = 10
    for e in range(EPISODES):
        score = 0
        done = False
        state = env.reset()
        state = np.reshape(state, [1, state_size])
        while not done:
            action = agent.get_action(state)
            next_state, reward, done = env.step(action)
            next_state = np.reshape(next_state, [1, state_size])
            state = next_state
            score += reward
