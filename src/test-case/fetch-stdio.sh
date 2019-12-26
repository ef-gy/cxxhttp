#!/bin/sh
# Test the `fetch` programme against a deliberately slow server on STDIO

rv="true"

# sleep for a while to make sure the server is initialised.
sleep 1

printf "running test case 1: "

uri="http://localhost:stdio/"
out="data/test/fetch/12123456789"
tmp="/tmp/cxxhttp-fetch-test-12123456789"

if (echo "HTTP/1.1 200 OK"; echo "Content-Length: 12"; echo; echo "12";
    sleep 1; echo "123456789") |\
  ./fetch "${uri}" >"${tmp}"; then
  if diff --ignore-all-space -u "${out}" "${tmp}"; then
    echo "OK"
  else
    echo "FAIL"
    rv="false"
  fi
else
  echo "FAIL"
  rv="false"
fi

for ex in gcda gcno; do
  for p in server fetch; do
    [ -f ${p}.${ex} ] && mv -f ${p}.${ex} fetch-stdio-${p}.${ex}
  done
done

exec ${rv}
