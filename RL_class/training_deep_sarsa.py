import copy
import pylab
import random
import numpy as np
from environment_deep_sarsa import Env
import tensorflow as tf
from tensorflow.keras.layers import Dense
from tensorflow.keras.optimizers import Adam
from pathlib import Path


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
        self.discount_factor = 0.99
        self.learning_rate = 0.001
        self.epsilon = 1.
        self.epsilon_decay = .9999
        # 교재 445~446p 누락된 초기화 보완
        self.epsilon_min = 0.01
        self.model = DeepSARSA(self.action_size)
        self.model(tf.zeros((1, self.state_size)))
        self.optimizer = Adam(learning_rate=self.learning_rate)

    def get_action(self, state):
        if np.random.rand() <= self.epsilon:
            return random.randrange(self.action_size)
        else:
            q_values = self.model(state)
            return np.argmax(q_values[0])

    def train_model(self, state, action, reward, next_state, next_action, done):
        if self.epsilon > self.epsilon_min:
            self.epsilon *= self.epsilon_decay
        model_params = self.model.trainable_variables
        with tf.GradientTape() as tape:
            tape.watch(model_params)
            predict = self.model(state)[0]
            one_hot_action = tf.one_hot([action], self.action_size)
            predict = tf.reduce_sum(one_hot_action * predict, axis=1)
            next_q = self.model(next_state)[0][next_action]
            target = reward + (1 - done) * self.discount_factor * next_q
            loss = tf.reduce_mean(tf.square(target - predict))
        grads = tape.gradient(loss, model_params)
        self.optimizer.apply_gradients(zip(grads, model_params))


if __name__ == '__main__':
    env = Env(render_speed=0.01)
    state_size = 15
    action_space = [0, 1, 2, 3]  # 환경 action_space 4개와 정합
    action_size = len(action_space)
    agent = DeepSARSAgent(state_size, action_size)
    scores, episodes = [], []
    EPISODES = 1000
    for e in range(EPISODES):
        done = False
        score = 0
        state = env.reset()
        state = np.reshape(state, [1, state_size])
        while not done:
            action = agent.get_action(state)
            next_state, reward, done = env.step(action)
            next_state = np.reshape(next_state, [1, state_size])
            next_action = agent.get_action(next_state)
            agent.train_model(state, action, reward, next_state, next_action, done)
            score += reward
            state = next_state
            if done:
                print('episode: {:3d} | score: {:3d} | epsilon: {:.3f}'.format(e, score, agent.epsilon))
        scores.append(score)
        episodes.append(e)
        pylab.plot(episodes, scores, 'b')
        pylab.xlabel('episode')
        pylab.ylabel('score')
        Path('save_graph').mkdir(exist_ok=True)
        pylab.savefig('./save_graph/graph_deep_sarsa.png')
        if e % 100 == 0:
            Path('save_model').mkdir(exist_ok=True)
            agent.model.save_weights('save_model/model_deep_sarsa.weights.h5')
