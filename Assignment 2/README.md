# CSC-360 Assignment 2

Program ACS (Airline check-in system) is used to simulate a two queue, 5 clerk airline check in system.

To use the program.
1. Run makefile ./Makefile
2. Create an input file in the form:

******
n
0:C,A,S
1:C1,A1,S1
.
.
.
n:Cn,An,Sn
*******

Where:
number on the left - The customer ID
C - customers class (What queue should the customer go in)
A - customers arrival time (What time does the customer arrive at the end of the queue in tenths of second)
S - customers service time (How long does the clerk have to spend servicing this customer in tenths of a second)

3. Run ACS with the input file from 2 ./ACS input.txt
4. Enjoy some chaotic simulating :)
