import numpy as np
import random
from collections import defaultdict
from environment_mc import Env


# 몬테카를로에이전트(모든에피소드각각의샘플로부터학습)
class MCAgent:
    def __init__(self, actions):
        self.width = 5
        self.height = 5
        self.actions = actions
        self.learning_rate = 0.01
        self.discount_factor = 0.9
        self.epsilon = 0.1
        self.samples = []
        self.value_table = defaultdict(float)

    # 메모리에샘플을추가
    def save_sample(self, state, reward, done):
        self.samples.append([state, reward, done])

    # 모든에피소드에서에이전트가방문한상태의큐함수를업데이트
    def update(self):
        G_t = 0
        visit_state = []
        for reward in reversed(self.samples):
            state = str(reward[0])
            if state not in visit_state:
                visit_state.append(state)
                G_t = reward[1] + self.discount_factor * G_t
                value = self.value_table[state]
                self.value_table[state] = (value +
                                           self.learning_rate * (G_t - value))

    # 큐함수에따라서행동을반환
    # 입실론탐욕정책에따라서행동을반환
    def get_action(self, state):
        if np.random.rand() < self.epsilon:
            # 랜덤행동
            action = np.random.choice(self.actions)
        else:
            # 큐함수에따른행동
            next_state = self.possible_next_state(state)
            action = self.arg_max(next_state)
        return int(action)

    # 후보가여럿이면arg_max를계산하고무작위로하나를반환
    @staticmethod
    def arg_max(next_state):
        max_index_list = []
        max_value = next_state[0]
        for index, value in enumerate(next_state):
            if value > max_value:
                max_index_list.clear()
                max_value = value
                max_index_list.append(index)
            elif value == max_value:
                max_index_list.append(index)
        return random.choice(max_index_list)

    # 가능한다음모든상태들을반환
    def possible_next_state(self, state):
        col, row = state
        next_state = [0.0] * 4

        if row != 0:
            next_state[0] = self.value_table[str([col, row - 1])]
        else:
            next_state[0] = self.value_table[str(state)]
        if row != self.height - 1:
            next_state[1] = self.value_table[str([col, row + 1])]
        else:
            next_state[1] = self.value_table[str(state)]
        if col != 0:
            next_state[2] = self.value_table[str([col - 1, row])]
        else:
            next_state[2] = self.value_table[str(state)]
        if col != self.width - 1:
            next_state[3] = self.value_table[str([col + 1, row])]
        else:
            next_state[3] = self.value_table[str(state)]

        return next_state


# 메인함수
if __name__ == "__main__":
    env = Env()
    agent = MCAgent(actions=list(range(env.n_actions)))

    for episode in range(1000):
        state = env.reset()
        action = agent.get_action(state)

        while True:
            env.render()
            # 다음상태로이동
            # 보상은숫자이고, 완료여부는boolean
            next_state, reward, done = env.step(action)
            agent.save_sample(next_state, reward, done)

            # 다음행동받아옴
            action = agent.get_action(next_state)
            env.print_value_all({s: [v] * 4 for s, v in agent.value_table.items()})
            # 에피소드가완료됐을때, 큐함수업데이트
            if done:
                agent.update()
                agent.samples.clear()
                break
