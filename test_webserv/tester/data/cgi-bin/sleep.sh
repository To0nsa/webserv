#!/usr/bin/env bash
# sleep.sh — sleeps past the server timeout, then writes headers + body

sleep 10

# once the sleep is over (but the server will have already timed out)...
echo -e "Content-Type: text/plain\r\n\r\n"
echo "This line will never be seen—server already sent 504"