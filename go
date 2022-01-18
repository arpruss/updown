killall monitor updown forcesleep
cd /data/local/tmp
chmod 755 monitor digitaleditions.sh updown forcesleep
./monitor com.adobe.digitaleditions ./digitaleditions.sh "killall updown forcesleep" > /dev/null 2> /dev/null &

