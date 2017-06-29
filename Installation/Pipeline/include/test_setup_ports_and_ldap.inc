PORTTRAP=""
LDAPTRAP=""

PORT01=`./Installation/Pipeline/port.sh`
PORTLDAP=`expr $PORT01 + 199`

PORTTRAP="./Installation/Pipeline/port.sh --clean $PORT01 ;"

if [ "$edition" == enterprise -a "$os" == linux ]; then
  docker rm -f ldap-$PORTLDAP || echo
  mkdir -p ldap
  docker pull arangodb/openldap-test-container:jessie
  docker stop ldap-$PORTLDAP
  docker run -d -e LDAP_CERT_CN=127.0.0.1 -p $PORTLDAP:389 -v $(pwd)/ldap:/cert --name ldap-$PORTLDAP arangodb/openldap-test-container:jessie

  LDAPTRAP="docker stop ldap-$PORTLDAP ;"
fi


trap "$LDAPTRAP $PORTTRAP" EXIT
