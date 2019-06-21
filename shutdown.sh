for i in 1 2 
do
   ./bin/rbd unmap rbd$i/image$i
done

../src/stop.sh
