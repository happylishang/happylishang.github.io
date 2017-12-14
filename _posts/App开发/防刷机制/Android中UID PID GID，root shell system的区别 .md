Android中UID PID GID，root shell system的区别

	-rwxr-x--- root     root         5339 1970-01-01 08:00 init.usb.rc
	-rwxr-x--- root     root          342 1970-01-01 08:00 init.zygote32.rc
	drwxr-xr-x root     system            1970-01-01 08:00 mnt

在Linux中，创建一个文件时，该文件的拥有者都是创建该文件的用户，该文件用户可以修改该文件的拥有者及用户组，当然root用户可以修改任何文件的拥有者及用户组。在Linux中，对于文件的权限（rwx），分为三部分，一部分是**该文件的拥有者所拥有的权限**，一部分是**该文件所在用户组的用户所拥有的权限**，另一部分是其**他用户所拥有的权限**。