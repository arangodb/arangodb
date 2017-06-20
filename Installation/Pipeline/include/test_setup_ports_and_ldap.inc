PORTTRAP=""
LDAPTRAP=""

PORTLDAP=""

if [ "$mode" == singleserver ]; then
  PORT01=`./Installation/Pipeline/port.sh`
  PORT02=`./Installation/Pipeline/port.sh`
  PORT03=`./Installation/Pipeline/port.sh`

  PORTLDAP=`expr $PORT03 + 199`

  PORTTRAP="./Installation/Pipeline/port.sh --clean $PORT01 $PORT02 $PORT03 ;"
elif [ "$mode" == cluster ]; then
  PORT01=`./Installation/Pipeline/port.sh`
  PORT02=`./Installation/Pipeline/port.sh`
  PORT03=`./Installation/Pipeline/port.sh`
  PORT04=`./Installation/Pipeline/port.sh`

  PORTLDAP=`expr $PORT04 + 199`

  PORTTRAP="./Installation/Pipeline/port.sh --clean $PORT01 $PORT02 $PORT03 $PORT04 ;"
fi

if [ "$edition" == enterprise ]; then
  docker rm -f ldap-$PORTLDAP || echo
  mkdir -p ldap
  docker pull arangodb/openldap-test-container:jessie
  docker stop ldap-$PORTLDAP
  docker run -d -e LDAP_CERT_CN=127.0.0.1 -p $PORTLDAP:389 -v $(pwd)/ldap:/cert --name ldap-$PORTLDAP arangodb/openldap-test-container:jessie

  LDAPTRAP="docker stop ldap-$PORTLDAP ;"
fi


trap "$LDAPTRAP $PORTTRAP" EXIT
