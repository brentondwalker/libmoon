#!/bin/bash
mkdir -p /mnt/huge
(mount | grep hugetlbfs | grep 2M) > /dev/null || mount -t hugetlbfs nodev /mnt/huge -o pagesize=2M
for i in {0..7}
do
	if [[ -e "/sys/devices/system/node/node$i" ]]
	then
		echo 512 > /sys/devices/system/node/node$i/hugepages/hugepages-2048kB/nr_hugepages
	fi
done

