# Renders docs/demo.gif with charmbracelet/vhs:
#   cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel
#   vhs docs/demo.tape
Output docs/demo.gif

Set Shell "bash"
Set FontSize 13
Set Width 1060
Set Height 540
Set Padding 16
Set Theme "Dracula"
Set TypingSpeed 25ms
Set Framerate 8

Hide
Type "export PATH=$PWD/build-rel:$PATH PS1='$ ' && cd /tmp && cp $OLDPWD/tests/fixtures/metu_ride.nmea . && clear"
Enter
Show

Type "nmea-logger -f metu_ride.nmea -r 120 --fence-circle ne-corner:39.8920,32.7860,60 --gpx ride.gpx --geojson ride.geojson"
Sleep 400ms
Enter
Sleep 6s
