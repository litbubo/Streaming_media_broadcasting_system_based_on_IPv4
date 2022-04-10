#! /bin/bash
sudo sysctl -w net.core.rmem_max=26214400 # 设置为 25M
# net.core.rmem_max = 26214400
sudo sysctl -w net.core.netdev_max_backlog=2000
# net.core.netdev_max_backlog = 2000
make

./client