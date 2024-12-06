cd bin
method=3
chunking=0
num=1
# name=LKT
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

# name=WEB
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

# name=chromium
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num

name=ThunderbirdTar
./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num
num=2

# name=LKT
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

# name=WEB
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num 

# name=chromium
# ./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num

name=ThunderbirdTar
./BiSearch -i /mnt/dataset2/$name -c $chunking -m $method -n $num

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