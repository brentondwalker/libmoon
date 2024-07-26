#!/bin/bash
mkdir -p /mnt/huge
(mount | grep hugetlbfs | grep 1024M) > /dev/null || mount -t hugetlbfs nodev /mnt/huge -o pagesize=1G
for i in {0..7}
do
	if [[ -e "/sys/devices/system/node/node$i" ]]
	then
		echo 10 > /sys/devices/system/node/node$i/hugepages/hugepages-1048576kB/nr_hugepages
	fi
done

