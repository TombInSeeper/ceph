./bin/ocssd_cli -d /dev/nvme0n1 -p 2 -m 1 -n 20

sleep 1

../src/vstart.sh -n -X -l --bluestore --nolockdep  mon osd --mon_num 1 --osd_num 2
