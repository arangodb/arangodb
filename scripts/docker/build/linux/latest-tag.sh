repo="arangodb/ubuntubuildarangodb-devel"
token=$(curl -s "https://auth.docker.io/token?service=registry.docker.io&scope=repository:${repo}:pull" | jq -r '.token')
latest=$(curl -H "Authorization: Bearer $token" -s "https://registry-1.docker.io/v2/${repo}/tags/list" | jq -r '.tags | values[]' | grep -e "^[0-9]*$" | sort -n | tail -1)

test ! -z "$latest" && echo $(expr $latest + 1)
