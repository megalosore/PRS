i=0
while [ $i -le 1000 ]
do
    echo $i
    ./client1 134.214.202.223 5000 file.txt 0
    sha256sum file.txt 
    sha256sum copy_file.txt 
    i=$((i+1))
done