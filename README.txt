Example 29:
No optimisation:
./out/example29 < data/example29.in > actual_output/example29.out -> 1.27s user 0.01s system 94% cpu 1.365 total

The first optimisation, which will be a good speedup on loops will be making the loop variable on any loop a
 local register, making accessing it much faster.



