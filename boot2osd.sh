OBJECT_STORE=memstore
OCSSD_ENABLE=1
OSD_NUM=2
PARTITION_MODE=1
BLK_NUM=300
RBD_SIZE_GB=20

if [ ${OCSSD_ENABLE} -eq 1 ] ; then 
	./bin/ocssd_cli -d /dev/nvme0n1 -p${OSD_NUM}  -m${PARTITION_MODE} -n${BLK_NUM}
	OBJECT_STORE=bluestore
fi

sleep 1

../src/vstart.sh -n -X -l --${OBJECT_STORE} --nolockdep  mon osd --mon_num 1 --osd_num ${OSD_NUM}

sleep 1
./bin/ceph osd pool rm rbd rbd --yes-i-really-really-mean-it

echo -e "Inject New crush map\n"

./bin/crushtool  -c ../newcrushmap.txt -o newcrushmap
./bin/ceph osd setcrushmap -i newcrushmap


for i in $(seq 1 $OSD_NUM)
do
	./bin/ceph osd pool create rbd${i} 8 8
	./bin/ceph osd pool set rbd${i} crush_ruleset ${i}
done

./bin/ceph osd tree

./bin/ceph osd pool ls

echo -e "create rbd device\n"

for i in $(seq 1 $OSD_NUM) 
do
	./bin/rbd create rbd${i}/image${i} --size ${RBD_SIZE_GB}G
	./bin/rbd feature disable rbd${i}/image${i} fast-diff
	./bin/rbd feature disable rbd${i}/image${i} object-map
	./bin/rbd feature disable rbd${i}/image${i} exclusive-lock
	./bin/rbd feature disable rbd${i}/image${i} deep-flatten
	./bin/rbd map rbd${i}/image${i} 
done

sleep 1

lsblk


