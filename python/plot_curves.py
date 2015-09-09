#!/bin/ipython

import sys
import matplotlib.pyplot as plt
import numpy as np
import string

basename = ""

def load_results(id):
    global basename
    basename = str(id) + "/stats/" + str(id).zfill(3) + "."

def isFloat(string):
    try:
        float(string)
        return True
    except ValueError:
        return False

def load_values(file, spp):
    f = open(file)
    lines = [ line for line in f ]
    values = [ float(x) for x in lines[0].split(' ') if isFloat(x) ]
    return values[:spp + 1]

def plot_curve(file, l, spp):
    values = load_values(file, spp)
    return plt.plot([ x for x in range(len(values)) ], values, label=l)

def plot_rmse(indices, spp):
    for i in indices:
        plot_curve(basename + str(i).zfill(3) + ".rmseFloat", str(i), spp)

def plot_processingTimes(indices, spp):
    for i in indices:
        plot_curve(basename + str(i).zfill(3) + ".processingTimes", str(i), spp)

def plot_rmse_of_time(indices, spp=sys.maxint):
	for i in indices:
	    rmse = load_values(basename + str(i).zfill(3) + ".rmseFloat", spp)
	    times = load_values(basename + str(i).zfill(3) + ".processingTimes", spp)
	    plt.plot(times, rmse, label=str(i))

def plot_rmse_of_time_enhanced(i1, i2, label1, label2, spp=sys.maxint):
    rmse = load_values(basename + str(i1).zfill(3) + ".rmseFloat", spp)
    times = load_values(basename + str(i1).zfill(3) + ".processingTimes", spp)
    plt.plot(times, rmse, label=label1, linewidth=2, color="red")
    rmse = load_values(basename + str(i2).zfill(3) + ".rmseFloat", spp)
    times = load_values(basename + str(i2).zfill(3) + ".processingTimes", spp)
    plt.plot(times, rmse, label=label2, linewidth=2, color="blue")

def plot_rmse_of_time_enhanced_log(i1, i2, label1, label2, spp=sys.maxint):
    rmse = load_values(basename + str(i1).zfill(3) + ".rmseFloat", spp)
    times = load_values(basename + str(i1).zfill(3) + ".processingTimes", spp)
    plt.semilogx(times[32:], rmse[32:], label=label1, linewidth=2, color="red", basex=10)
    rmse = load_values(basename + str(i2).zfill(3) + ".rmseFloat", spp)
    times = load_values(basename + str(i2).zfill(3) + ".processingTimes", spp)
    plt.semilogx(times[32:], rmse[32:], label=label2, linewidth=2, color="blue", basex=10)

def plot_rmse_of_spp(indices, spp=sys.maxint):
    for i in indices:
        rmse = load_values(basename + str(i).zfill(3) + ".rmseFloat", spp)
        plt.plot(range(len(rmse)), rmse, label=str(i))

def compare(i1, i2, spp=sys.maxint):
    names = [i1, i2]
    rmse = [ [], [] ]
    times = [ [], [] ]

    for i in range(2):
        rmse[i] = load_values(basename + str(names[i]).zfill(3) + ".rmseFloat", spp)
        times[i] = load_values(basename + str(names[i]).zfill(3) + ".processingTimes", spp)
        plt.plot(times[i], rmse[i], label=str(names[i]))

    spp = min(len(rmse[0]) - 1, spp)
    spp = min(len(rmse[1]) - 1, spp)

    print("spp = ", spp)

    if rmse[0][0] < rmse[1][0]:
        best = 0
        worst = 1
    else:
        best = 1
        worst = 0
    print("at spp = 0 best is " + str(names[best]))
    print("time for best is " + str(times[best][0]))
    print("time for worst is " + str(times[worst][0]))
    for i in range(spp):
        if(rmse[worst][i] < rmse[best][i]):
            best, worst = worst, best
            print("at spp = " + str(i) + " best is " + str(names[best]))
            print("time for best is " + str(times[best][i]))
            print("time for worst is " + str(times[worst][i]))


def show_all():
    l = plt.legend()
    plt.show()