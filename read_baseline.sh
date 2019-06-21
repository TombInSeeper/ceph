
for i in 1 2 4
do
	./bin/ocssd_cli -p$i -n100 -m1 -d/dev/nvme0n1 > /dev/null
	./bin/ocssd_test $i r
done

for i in 1 2 4
do
        ./bin/ocssd_cli -p$i -n100 -m2 -d/dev/nvme0n1 > /dev/null
        ./bin/ocssd_test $i r
done

