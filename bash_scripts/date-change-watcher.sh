#!/bin/bash

#INCOMPLETE
#TODO: Make it email on change, and make sleep time confirgurable

if [[ $# -ne 1 ]]; then
    echo "Requires at least one parameter (page to check)"
    exit 0
else
    if [[ ! $1 == http*://* ]]; then
        echo "Requires a link that starts with http:// or https://"
        exit 0
    fi
    WEB_URL=$1
fi

# Silent mode for no error output or stats (-s)
# Follow redirects (-L)
# Five second timeout (-m 5)
# Allow insecure SSL certs (-k)
# Print status code at the end (-w "%{http_code}")
CURL_FLAGS="-s -L -k -m 5 -H 'Cache-Control: no-cache' -w \n"%{http_code}""

# Up to 5 characters before match, and 9 characters after
#   - 5 characters before: In cases of "15th SEPtember"
#   - 9 characters after: In cases of "SEPtember 15"
GREP_PATTERN='.{0,5}(Jan|Feb|Mar|Apr|Jun|Jul|Aug|Sep|Oct|Nov|Dec).{0,9}'

PAGE_RESP=`curl ${CURL_FLAGS} ${WEB_URL}`
OLD_HASH=`echo "${PAGE_RESP}" | grep -o -E ${GREP_PATTERN} | grep -E ' [0-9]' | md5sum`
sleep 10m

while [ True ]; do
    PAGE_RESP=`curl ${CURL_FLAGS} ${WEB_URL}`
    echo "=== TLIN DEBUG"
    echo "${PAGE_RESP}" | grep -o -E ${GREP_PATTERN} | grep -E ' [0-9]' # TLIN DEBUG
    echo "=== END TLIN DEBUG"
    STATUS=`echo "${PAGE_RESP}" | tail -n 1`

    if [[ "${STATUS}" == "200" ]]; then
        HASH=`echo "${PAGE_RESP}" | grep -o -E ${GREP_PATTERN} | grep -E ' [0-9]' | md5sum`
        if [[ ${HASH} != ${OLD_HASH} ]]; then
            OLD_HASH=${HASH}
            echo "DATE CHANGED! Changed on" `date`

            DATE=`echo "${PAGE_RESP}" | grep -o -E ${GREP_PATTERN} | grep -E ' [0-9]'`
            echo ${DATE}
            echo

            echo -e "${WEB_URL}\n\n${DATE}" | mail -s "Conference Deadline Changed" kindom.maps@gmail.com
            #echo -e "${WEB_URL}\n\n${DATE}" | mail -s "Conference Deadline Changed" simona.marinova112@gmail.com
            #echo -e "${WEB_URL}\n\n${DATE}" | mail -s "Conference Deadline Changed" iman.tabrizian@gmail.com
        fi
    fi

    sleep 5m
done
