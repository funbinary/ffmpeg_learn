#!/bin/bash -x
workDir=$(cd $(dirname $0); pwd)

cd ${workDir}/build
cmake ..
make -j8

sshPassword=123
remotePath=/root
remoteAddr=192.168.3.98
target=${workDir}/build/cameraToRtmp
#target=${workDir}/build/cameraToh264
#target=${workDir}/build/ffmpeg_learn
#target=${workDir}/build/flvToRtmp


sshpass -p $sshPassword scp $target root@$remoteAddr:$remotePath
