cd ../src/
make
cd ../etc/
python scripts/run.py 'valgrind --leak-check=full --show-reachable=yes'
sleep 10000
