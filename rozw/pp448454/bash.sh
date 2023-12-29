
for (( i=0; i<1000000; i++ ))
do
    ./mimpirun 2 test1
    
    # BEGIN: ed8c6549bwf9
    if (( i % 1000 == 0 )); then
        echo "Test $i"
    fi
    # END: ed8c6549bwf9
done