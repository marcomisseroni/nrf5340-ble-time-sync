import pandas as pd
import numpy as np
import os
import matplotlib.pyplot as plt

'''
This script is meant to produce a plot depicting the relationship
between the standard deviation of the synchronization error and the 
connection interval
'''

df = pd.read_csv("experiments/std_summary.csv")
df.plot(x='ci_ms', y=['std_pc', 'std_pp'], style='o')
plt.show()