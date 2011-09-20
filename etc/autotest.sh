python ../test/test_fuzzy.py
#python ../test/test_mem.py
python ../test/test_protocol.py
rm -f ../test/test_slab
rm -f ../test/test_util
gcc -std=c99 -pedantic -Wall -W -lm ../test/test_base.c ../test/test_slab.c ../src/slab.c -o ../test/test_slab -D LC_TEST -I ../src/ && ../test/test_slab
gcc -std=c99 -pedantic -Wall -W -lm ../test/test_base.c ../test/test_util.c ../src/util.c -o ../test/test_util -D LC_TEST -I ../src/ && ../test/test_util
echo "*** AUTOTESTS finished."
sleep 10000
