#!/bin/sh
#$1 should be p10 or p20

board=86v
chip=fullmask
imgtype=rel

git branch > /dev/null 2>&1
if [ ! $? -eq 0 ] ; then
	echo "dose not exist git base"
	tag="0.0"
	branch="default"
else
	# branch=`git branch  -r | grep "\->" |awk -F "/" 'END{print $NF}'`
	local_branch_name=`git branch -vv |grep "*" |awk '{print $2}'`
	# echo "local branch name $local_branch_name"
	if [ "$local_branch_name" = "(no" ]; then
		echo "branch set fail no local branch"
		exit 1
	fi

	branch=`git branch -vv |grep "*" | awk -F "[][]" '{print $2}'| awk -F "[/:]" '{print $2}'`
	echo "branch is $branch"

	# handle release branch case release branch name is not release!!!
	is_86v_release=$(echo $branch | grep "release.86v")
	isrelease=$(echo $branch | grep "release")
	if [ "$is_86v_release" != "" ];then
		branch="release.86v"
	elif [ "$isrelease" != "" ];then
		branch="release"
	fi
	tag=`git tag  |  grep "${branch}-" | sort -V | awk 'END{print}'`
	if [ ! -n "$tag" ] ;then
		#compatible with old version
		tag=`git tag  |  grep -v "v"  | sort -V | awk 'END{print}'`
		version=$tag
	else
		version=`printf "$tag" | awk -F "[-]" '{print $2}'`
	fi
	echo "version is $version"
	tag_commit=`git show $tag|grep ^commit | awk '{print substr($2,0,7)}'`
	echo "tag commit $tag_commit"
	last_commit=`git rev-parse --short HEAD`
	echo "last commit $last_commit"

	if [ $tag_commit != $last_commit ]; then
		commit_suffix=$last_commit
	fi
fi


#$1 p10 p10m p20 86v clean default p10m
if [ -n "$1" ]; then
	board=$1
fi

#$2 shoule be mpw0 mpw1 fullmask default fullmask
if [ -n "$2" ]; then
	chip=$2
fi

#$3 shoule be auto/flash/rel  default rel
if [ -n "$3" ]; then
	imgtype=$3
fi

echo "build board is $board chip $chip imgtype $imgtype"

if [ -n "$commit_suffix" ]; then
	prefix=openwrt_${branch}_${board}_${chip}_${imgtype}_${version}_${commit_suffix}
else
	prefix=openwrt_${branch}_${board}_${chip}_${imgtype}_${version}
fi

target_bin="bin/siflower/openwrt-siflower-sf16a18-${chip}-squashfs-sysupgrade.bin"

echo "prefix is ${prefix}"
#set up version.mk
rm include/siwifi_version.mk
echo "VERSION_DIST:=SiWiFi" >> include/siwifi_version.mk
echo 'VERSION_NICK:=$(PROFILE)' >> include/siwifi_version.mk
echo "VERSION_NUMBER:=${prefix}" >> include/siwifi_version.mk

sed -e '12cVERSION_NUMBER:='${prefix}'' include/siwifi_version.mk > tmp_version

sed -e '14cVERSION_NUMBER:='${prefix}'' tmp_version > include/siwifi_version.mk

rm tmp_version

echo "set openwrt version"

case ${board} in
	p10_8m)
		target_board=target/linux/siflower/sf16a18_p10_${chip}_8m.config
		;;
	p10)
		target_board=target/linux/siflower/sf16a18_p10_${chip}_def.config
		;;
	p10m)
		target_board=target/linux/siflower/sf16a18_p10_${chip}_gmac.config
		;;
	p20)
		target_board=target/linux/siflower/sf16a18_evb_v5_${chip}_def.config
		;;
	86v)
		target_board=target/linux/siflower/sf16a18_86v_${chip}_def.config
		;;
	86v_c2)
		target_board=target/linux/siflower/sf16a18_86v_c2_${chip}_def.config
		;;
	rep)
		target_board=target/linux/siflower/sf16a18_rep_${chip}_def.config
		;;
	rep_nopa)
		target_board=target/linux/siflower/sf16a18_rep_nopa_${chip}_def.config
		;;
	ac)
		target_board=target/linux/siflower/sf16a18_ac_${chip}_def.config
		;;
	x10)
		target_board=target/linux/siflower/sf16a18_p10_${chip}_x10.config
		;;
	p10h)
		target_board=target/linux/siflower/sf16a18_p10h_${chip}_gmac.config
		;;
	evb_v5)
		target_board=target/linux/siflower/sf16a18_evb_v5_${chip}_def.config
		;;
	cpe)
		target_board=target/linux/siflower/sf16a18_cpe_${chip}_def.config
		;;
	clean)
		echo "clean build enviroment"
		echo "delete build dir "
		rm -rf build_dir
		echo "delete tmp"
		rm -rf tmp
		echo "delete board"
		rm .board
		exit 1
		;;
	*)
		echo "error arg"
		exit 1 # Command to come out of the program with status 1
		;;
esac


echo "set up board $target_config"

if [ "$imgtype" = "flash" ]; then
	target_board=target/linux/siflower/sf16a18_p10_${chip}_flash.config
fi

if [ -f .board ] && [ "$imagetype" != "auto" ]; then
	cmp_reselt=`cmp $target_board .config`
	if [ -n "$cmp_reselt" ]; then
		echo "board change, clean build enviroment"
		rm -rf build_dir
		rm -rf tmp
		cp $target_board .config
	fi
else
	cp $target_board .config
fi

rm $target_bin

case ${imgtype} in
	auto)
		echo "CONFIG_PACKAGE_autotest=y" >> .config
		echo "build auto"
		;;

	flash)
		echo "build flash"
		;;
	rel)
		echo "build release"
		;;
	*)
		echo "imgtype error arg"
		exit 1 # Command to come out of the program with status 1
		;;
esac

make package/base-files/clean
make -j32 V=s

if [ -f $target_bin ]; then
	cp $target_bin ${prefix}.bin
else
	echo "build fail, don't get target_bin"
fi

echo "build end"
