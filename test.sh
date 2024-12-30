cd bin
method=1
chunking=0
num=1
# Instructions=BiSearch
Instructions=Delta
name=LKT
./$Instructions -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

name=WEB
./$Instructions -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

name=chromium
./$Instructions -i /mnt/dataset2/$name -c $chunking -m $method -n $num

name=CentOS
./$Instructions -i /home/public/SXL/data/dataGeneration/multiuser_modifications -c $chunking -m $method -n $num

num=2

name=LKT
./$Instructions -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

name=WEB
./$Instructions -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

name=chromium
./$Instructions -i /mnt/dataset2/$name -c $chunking -m $method -n $num

name=CentOS
./$Instructions -i /home/public/SXL/data/dataGeneration/multiuser_modifications -c $chunking -m $method -n $num

# name=LKT
# num=2
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num  >C"${chunking}M${method}_${name}".txt

# name=WEB
# num=2
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num  >C"${chunking}M${method}_${name}".txt

# name=chromium
# num=2
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num  >C"${chunking}M${method}_${name}".txt

# name=ThunderbirdTar
# num=2
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num  >C"${chunking}M${method}_${name}".txt