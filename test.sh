make
for i in {0..7};
do
     ./AcDc ../test/testdata/in/eopxd$i.ac ../test/testdata/tall_out/out$i.out > /dev/null
done
red=`tput setaf 1`
green=`tput setaf 2`
for i in {0..7};
do
    result=$(diff ../test/testdata/tall_out/out$i.out ../test/testdata/out/eopxd$i.dc)
    if [ "$result" != "" ]
    then
        echo ${red} "file $i is different"
    else
        echo ${green} "file $i is the same"
    fi
done

tput sgr0

