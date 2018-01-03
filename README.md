This is to find the newest files in a directory tree, or the oldest, as the case may be, where newest is defined by 'latest mtime' by default.  This can be changed to ctime or atime.  

I'd like to thank FreeBSD 4.11's find for not having printf0, which can also do this quite nicely, and which led me to just say I want my own. 

```bash
newest --help
Usage: ./src/newest [OPTION]... [DIRECTORY]...
Print the newest file in a directory tree to standard output
By default they are displayed in descending order of last modified time.
  -a, --atime              sort by access time  
  -m, --mtime              sort by modified time 
  -c, --ctime              sort by inode update time (so-called creation time) 
  -n, --number=N           show the N newest files 
  -R, --reverse            show the oldest files instead 
  -d, --include-dirs       include directories in list instead of only plain files 
  -q, --quiet              suppress timestamp info.  show only filenames 
  -H, --human              print timestamp in more friendly format than epoch time 
  -e, --ignore-empty       don't include empty files in the results 
  -h, --help               display this help message and exit 
  -v, --version            display version information and exit 
```

```bash
newest -H -n 5 /home/dallas/Dropbox/Pictures 
findin' newest 5 in /home/dallas/Dropbox/Pictures son
showing 5 results
/home/dallas/Dropbox/Pictures/Backgrounds/werner.jpg	Sun 24 Dec 2017 09:16:38 AM MST
/home/dallas/Dropbox/Pictures/Funny/pimpcchild.png	Thu 21 Dec 2017 06:50:45 PM MST
/home/dallas/Dropbox/Pictures/2017-12-10 Scottsdale Half Marathon/scottsdale_half_finish.mp4	Tue 19 Dec 2017 03:12:40 PM MST
/home/dallas/Dropbox/Pictures/wine/Photo Dec 11, 7 05 19 AM.jpg	Mon 11 Dec 2017 05:27:29 PM MST
/home/dallas/Dropbox/Pictures/wine/Photo Dec 11, 7 05 14 AM.jpg	Mon 11 Dec 2017 05:27:19 PM MST
```
