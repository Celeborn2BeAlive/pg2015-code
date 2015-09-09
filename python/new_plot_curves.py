# -*- coding: utf-8 -*-
"""
Created on Mon Apr 20 13:15:40 2015

@author: c2ba
"""

import sys
import matplotlib.pyplot as plt
import numpy as np
import string
import skimage
import skimage.io
import skimage.viewer
import sys
import functools

def is_float(string):
    try:
        float(string)
        return True
    except ValueError:
        return False

def load_values(file):
    f = open(file)
    lines = [ line for line in f ]
    values = [ float(x) for x in lines[0].split(' ') if is_float(x) ]
    return np.array(values, dtype='float')

def get_color(n):
    values = np.abs(np.modf(np.sin((n + 3) * np.array([12.9898, 78.233, 53.128])) * 43758.5453)[0])
    return (values[0], values[1], values[2])

error_label = {
    "nrmse": "NRMSE",
    "rmse": "RMSE",
    "mse": "L2 error",
    "mae": "L1 error"
}

class ResultStatistics:
    def __init__(self, index, basedir = "", label = ""):
        self.index = index
        self.basename = str(index).zfill(3)
        if not label:
            self.label = self.basename
        else:
            self.label = label
        self.error_values = {
                "nrmse": load_values(basedir + self.basename + ".nrmseFloat"),
                "rmse": load_values(basedir + self.basename + ".rmseFloat"),
                "mae": load_values(basedir + self.basename + ".absErrorFloat")
            }
        self.error_values["mse"] = np.square(self.error_values["rmse"])
        self.processing_times = load_values(basedir + self.basename + ".processingTimes") / 1000.
    
    def get_error_values(self, error_type):
        return self.error_values[error_type]

    def plot_error_curve_of_time(self, error_type, plot):
        plot.plot(self.processing_times, self.error_values[error_type], color=get_color(self.index), label = self.label, linewidth=1.5)
        
    def plot_error_curve_of_iterations(self, error_type, plot):
        plot.plot(range(len(self.error_values[error_type])), self.error_values[error_type], color=get_color(self.index), label = self.label, linewidth=1.5)

    def plot_efficiency(self, error_type):
        plt.plot(range(len(self.processing_times)), (1 / (self.error_values[error_type] * self.processing_times)), 
                 color=get_color(self.index), label = self.label)
    
    def plot_error_derivative(self, error_type):
        errors =self.get_error_values(error_type)
        derivative = [(errors[i + 1] - errors[i]) / self.processing_times[i + 1] for i in range(len(errors) - 1)]
        plt.plot(self.processing_times[1:], derivative, color=get_color(self.index), label = self.label)

def load_result(index, label, basedir = ""):
    return ResultStatistics(index, basedir, label)

def load_all_results(basedir = ""):
    l = []
    done = False
    i = 0
    while not done:
        try:
            r = load_result(i, "", basedir)
            l.append(r)
            i = i + 1
        except:
            done = True
    return l

def load_results(index_list, label_list, basedir = ""):
    return [load_result(index_list[i], label_list[i], basedir) for i in range(len(index_list))]

def plot_error_curves_of_time(result_list, error_type, time_range = [], error_range = [], title = ""):
    fig = plt.figure()
    plot = fig.add_subplot(111)
    for axis in [plot.xaxis, plot.yaxis]:
        for tick in axis.get_major_ticks():
            tick.label.set_fontsize(14)
    plot.grid(True)
    plot.set_ylabel(error_label[error_type], fontsize=16)
    plot.set_xlabel("time (seconds)", fontsize=16)
    if time_range:
        plot.set_xlim(time_range)
    if error_range:
        plot.set_ylim(error_range)
    for r in result_list:
        r.plot_error_curve_of_time(error_type, plot)
    legend = plot.legend(title = title, prop = {'size': 16})
    plt.setp(legend.get_title(),fontsize=32)

def plot_error_curves_of_iterations(result_list, error_type, it_range = [], error_range = [], title = ""):
    fig = plt.figure()
    plot = fig.add_subplot(111)
    for axis in [plot.xaxis, plot.yaxis]:
        for tick in axis.get_major_ticks():
            tick.label.set_fontsize(14)
    plot.grid(True)
    plot.set_ylabel(error_label[error_type], fontsize=16)
    plot.set_xlabel("iteration count", fontsize=16)
    if it_range:
        plot.set_xlim(it_range)
    if error_range:
        plot.set_ylim(error_range)
    for r in result_list:
        r.plot_error_curve_of_iterations(error_type, plot)
    legend = plot.legend(title = title, prop = {'size': 16})
    plt.setp(legend.get_title(),fontsize=32)

def compute_time_to_reach(results_list, error_type, target_error, ref_index = 0):
    times = []
    for result in results_list:
        error_values = result.get_error_values(error_type)
        time = -1
        for i in range(len(error_values)):
            if(error_values[i] < target_error):
                time = result.processing_times[i]
                break
        times.append(time)
    speedups = [ times[ref_index] / t for t in times]        
        
    return (times, speedups)

def compute_error_at_time(results_list, error_type, time):
    errors = []
    for result in results_list:
        times = result.processing_times
        error = -1
        for i in range(len(times)):
            if(times[i] > time):
                error = result.get_error_values(error_type)[i]
                break
        errors.append(error)
    return errors

def maximal_error(results_list, error_type):
    return max((r.get_error_values(error_type)[0] for r in results_list))

def minimal_error(results_list, error_type):
    return max((r.get_error_values(error_type)[-1] for r in results_list))

def plot_efficiency(result_list, error_type, iteration_count = -1):
    plt.grid(True)
    if iteration_count > 0:
        plt.xlim([0, iteration_count])
    for r in result_list:
        r.plot_efficiency(error_type)
    plt.legend()

def get_best_result(result_list, error_type):
    best_error = sys.float_info.max
    best_result = -1
    for i in range(len(result_list)):
        error = result_list[i].get_error_values(error_type)[-1]
        if error < best_error:
            best_error = error
            best_result = i
    if best_result == -1:
        return -1
    return result_list[best_result].index

def get_sorted_index_list_by_error(result_list, error_type):
    return sorted(range(len(result_list)), key=functools.cmp_to_key(lambda i,j: result_list[i].get_error_values(error_type)[-1] - result_list[j].get_error_values(error_type)[-1]))

results = load_all_results("stats/")
plot_error_curves_of_time(results, "mae")
plt.show()
plot_efficiency(results, "mse", min((len(result.processing_times) for result in results)))
plt.show()

print("L1 (MAE) error = ", [results[i].index for i in get_sorted_index_list_by_error(results, "mae")])
print("L2 (MSE) error = ", [results[i].index for i in get_sorted_index_list_by_error(results, "mse")])
print("RMSE error = ", [results[i].index for i in get_sorted_index_list_by_error(results, "rmse")])
print("NRMSE error = ", [results[i].index for i in get_sorted_index_list_by_error(results, "nrmse")])

mae_list = get_sorted_index_list_by_error(results, "mae")
plot_error_curves_of_time([results[mae_list[0]], results[0]], "mae") # compare BPT and the best result on a curve
plt.show()

results[0].label = "BPT";
results[2].label = "SkelBPT";
results[1].label = "ICBPT";

plot_error_curves_of_time([results[0], results[1], results[2]], "mae", title="Sponza", time_range=[0,300])
plt.show()

print(compute_time_to_reach(results, "mae", 0.025))

#plot_error_curves_of_iterations([results[0], results[1], results[2]], "mae", it_range=[0,1000], title="Sponza")
#plt.show()

#error_range = (maximal_error([results[0], results[1], results[2]], "mae")
#    , minimal_error([results[0], results[1], results[2]], "mae"))
#
#error_diff = abs(error_range[0] - error_range[1])
#
#target_error_count = 1000
#
#delta = error_diff / target_error_count
#
#times_list = []
#speedups_list = []
#
#for i in range(target_error_count):
#    target_error = error_range[0] - i * delta
#    times, speedups = compute_time_to_reach([results[0], results[1], results[2]], "mae", target_error);
#    times_list.append(times)
#    speedups_list.append(speedups)
#
#best_speedup_index = -1
#best_speedup = 0
#
#for i in range(len(speedups_list)):
#    if speedups_list[i][2] > best_speedup:
#        best_speedup_index = i
#        best_speedup = speedups_list[i][2]
#
#print("best speedup = ", speedups_list[best_speedup_index], " at time = ", times_list[best_speedup_index], " for error = ",
#      error_range[0] - best_speedup_index * delta)