feh --bg-center ~/background.png
pipewire &
pipewire-pulse &
sleep 5 && wireplumber &
dwmstatus &
exec dwm
