
## Overview

This document states fuctionalities when running the solution program for Exercise 2 and 3, as well as answers for the theoretical questions. 

## Github username and Tags for Git Tutorial

    -Username : alafnayou
    -Tags : 
            -Coding.Challenge.2
            -Coding.Challenge.3


## Instructions for Exercise 2 and 3

    Main functionalities: 

    - Press Q to quit.
    - Press E to switch states/screens.

    - First screen:

            -Press A, B, C or D to increase the respective value by 1.

            -Press Right Mouse Click to reinitialize values back to 0.

    - Second screen:

            -Press UP to trigger task that counts number of times UP was pressed.

            -Press DOWN to trigger task that counts number of times DOWN was pressed.

            -Press RIGHT to pause/resume Timer on the right side of the screen.

    - Third Screen:

            - No Presses


## 
 
## Answers to Theoretical Questions:


2.3.1. How does the mouse guarantee thread-safe functionality ?
    - The mouse can guarantee thread-safe functionality through Atomic operations.
    The shared data (for example Mouse Position) is accessed by using atomic operations which cannot be interrupted by other threads. Since the operations are atomic, 
    the shared data is always kept in a valid state, no matter how other threads access it.

3.1.1. What is the kernel tick ?
    -The kernel tick determines how often the kernel should check what task is running, what task is ready to run, along with task scheduling.

3.1.2. What is a tickless kernel ?
    -A tickless kernel is an operating system kernel in which timer interrupts do not occur at regular intervals and constant frequency but are only delivered when required.

3.2.2.5. What happens if Task Stack Size is too low ?
    -The processor context is saved onto a task�s stack each time the scheduler temporarily stops running the task in order to run a different task. 
    So when the stack size is too low, the task can run out of free stack space pretty fast and that can lead to undefined behaviour.

3.3.3. What can you observe when play around with the priorities from exercise in Section 3.3.2 ?
    -The order of the displayed digits at each tick changes with every new combination of priorities set to each task.