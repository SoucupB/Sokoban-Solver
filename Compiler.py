import os
stref = "'ccall'"

os.system(f'em++ -s NO_EXIT_RUNTIME=1 -s "EXTRA_EXPORTED_RUNTIME_METHODS=[{stref}]" -s ALLOW_MEMORY_GROWTH -s TOTAL_MEMORY=4GB main.cpp -o sokoban.html -O3')