#!/bin/bash -x

BASE_DIR=/newtemp/research/ditto
SRC_DIR=${BASE_DIR}/dot_snap_20070206

INSTALL_BASE_DIR=${BASE_DIR}/emulab_map

INSTALL_DIR=${BASE_DIR}/install

SCRIPTS_DIR=${BASE_DIR}/test/scripts/build_deploy_and_run
BASE_TAR_DIR=${BASE_DIR}/tar
PERL_SCRIPTS_DIR=${BASE_DIR}/test/mobisys

#EMULAB_USER_NAME=amar
EMULAB_USER_NAME=fdogar

#EMULAB_EXPT_NAME=nodew1.ditto.netarch.emulab.net
EMULAB_EXPT_NAME=nodew1.ditto.cmu849.emulab.net

DST_DIR=mobisys/
#DST_DIR=project/ditto/


#for testbed in "emulab" "map"
#for testbed in "emulab"
for testbed in "map"
do

    TAR_DIR=${BASE_TAR_DIR}/$testbed
    rm -rf ${TAR_DIR}/*
    mkdir -p ${TAR_DIR}/dot/build/gtcd
    mkdir -p ${TAR_DIR}/dot/build/gcp
    mkdir -p ${TAR_DIR}/dot/conf
    
    if [ $testbed = "emulab" ]
    then
        echo "emulab!";
        ### SCP Scripts to Emulab
        scp ${PERL_SCRIPTS_DIR}/* ${EMULAB_USER_NAME}@${EMULAB_EXPT_NAME}:~/${DST_DIR}
        scp -r ${PERL_SCRIPTS_DIR}/setup ${EMULAB_USER_NAME}@${EMULAB_EXPT_NAME}:~/${DST_DIR}
        scp -r ${PERL_SCRIPTS_DIR}/emulab_olsr_setup ${EMULAB_USER_NAME}@${EMULAB_EXPT_NAME}:~/${DST_DIR}
    fi

   if [ $testbed = "map" ]
   then
       echo "map!";
       ### SCP Scripts to MAP
       scp ${PERL_SCRIPTS_DIR}/* ditto@sp01.ecn.purdue.edu:/home/ditto/cmu/
       scp -r ${PERL_SCRIPTS_DIR}/setup ditto@sp01.ecn.purdue.edu:/home/ditto/cmu/
   fi

    #for chunksize in "8K" "16K" "32K"
	for chunksize in "16K"
     
	  do

        #INSTALL_DIR=${INSTALL_BASE_DIR}/$testbed/dot_$chunksize
        cp ${SCRIPTS_DIR}/emulab_olsr_setup/setup-olsr.sh ${INSTALL_DIR}/bin/

        cp ${INSTALL_DIR}/sbin/gtcd_$chunksize ${TAR_DIR}/dot/build/gtcd
        cp ${SRC_DIR}/olsr_route.sh ${TAR_DIR}/dot/build/gtcd
        cp ${INSTALL_DIR}/bin/gcp ${TAR_DIR}/dot/build/gcp
        cp ${INSTALL_DIR}/bin/sniffTcp_$chunksize ${TAR_DIR}/dot/build
        cp ${SCRIPTS_DIR}/dot.conf ${TAR_DIR}/dot/conf 

    done
    cd ${TAR_DIR}
    tar -czf all.tar.gz dot

    if [ $testbed = "emulab" ]
    then
        echo "emulab!";
        ### SCP TARBALL to Emulab
        scp ${TAR_DIR}/all.tar.gz ${EMULAB_USER_NAME}@${EMULAB_EXPT_NAME}:~/${DST_DIR}
    fi

    if [ $testbed = "map" ]
    then
        echo "map!";
        ### SCP TARBALL to MAP
        scp ${TAR_DIR}/all.tar.gz ditto@sp01.ecn.purdue.edu:/home/ditto/cmu/
    fi

    cd -

done

### SCP TARBALL to Emulab
# scp ${PERL_SCRIPTS_DIR}/* fdogar@nodew1.ditto.cmu849.emulab.net:~/project/mobisys/
# scp -r ${PERL_SCRIPTS_DIR}/setup fdogar@nodew1.ditto.cmu849.emulab.net:~/project/mobisys/
# scp -r ${PERL_SCRIPTS_DIR}/emulab_olsr_setup fdogar@nodew1.ditto.cmu849.emulab.net:~/project/mobisys/
# scp ${TAR_DIR}/*all.tar.gz fdogar@nodew1.ditto.cmu849.emulab.net:~/project/mobisys/


### SCP stuff to Emulab
# scp -r ${INSTALL_DIR}/sbin/* fdogar@nodew1.ditto.cmu849.emulab.net:~/project/dot/sbin/
# scp -r ${INSTALL_DIR}/bin/* fdogar@nodew1.ditto.cmu849.emulab.net:~/project/dot/bin/

# scp ${SRC_DIR}/olsr_route.sh fdogar@nodew1.ditto.cmu849.emulab.net:~/project/dot/sbin/
# scp ${SCRIPTS_DIR}/dot.conf fdogar@nodew1.ditto.cmu849.emulab.net:~/project/dot/sbin/
