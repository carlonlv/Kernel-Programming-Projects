This project achieves the goal of hijacking (intercepting) system calls by writing and installing a very basic kernel module to the Linux kernel. 

Here is what "hijacking (intercepting) a system call" means. A new system call is implemented named my_syscall, which will allow you to send commands from userspace, to intercept another pre-existing system call (like read, write, open, etc.). After a system call is intercepted, the intercepted system call would log a message first before continuing performing what it was supposed to do. 

For example, if we call my_syscall with command REQUEST_SYSCALL_INTERCEPT and target system call number __NR_mkdir (which is the macro representing the system call mkdir) as parameters, then the mkdir system call would be intercepted; then, when another process calls mkdir, mkdir would log some message (e.g., "muhahaha") first, then perform what it was supposed to do (i.e., make a directory). 

But wait, that's not the whole story yet. Actually we don't want mkdir to log a message whenever any process calls it. Instead, we only want mkdir to log a message when a certain set of processes (PIDs) are calling mkdir. In other words, we want to monitor a set of PIDs for the system call mkdir. Therefore, we can keep track, for each intercepted system call, of the list of monitored PIDs. Our new system call will support two additional commands to add/remove PIDs to/from the list. 
When we want to stop hijacking a system call (let's say mkdir but it can be any of the previously hijacked system calls), we can invoke the interceptor (my_syscall), with a REQUEST_SYSCALL_RELEASE command as an argument and the system call number that we want to release. This will stop intercepting the target system call mkdir, and the behaviour of mkdir should go back to normal like nothing happened. 

Functionalities:

1. REQUEST_SYSCALL_INTERCEPT and REQUEST_SYSCALL_RELEASE.

When an intercept command is issued, the corresponding entry in the system call table will be replaced with a generic interceptor function (discussed later) and the original system call will be saved. When a REQUEST_SYSCALL_RELEASE command is issued, the original saved system call is restored in the system call table in its corresponding position. 

2. REQUEST_START_MONITORING and REQUEST_STOP_MONITORING

Monitoring a process consists of the module logging into userspace some information about the process and the system call: the system call number, the parameters of the system call, and the pid of the process. 
When a REQUEST_START_MONITORING command comes through our custom system call, the kernel module must record internally that the pid passed as a parameter should be monitored for the syscall number (passed as a parameter as well). The monitoring can be done for a specific pid, or for all pids (in which case the pid parameter for my_syscall will be 0). 
PLEASE DO NOT DIRECTLY INSTALL THIS MODULE ON YOUR OWN MACHINE! TEST THIS CODE WITH A VIRTUAL MACHINE TO PREVENT DAMAGE TO YOUR OPERATING SYSTEM!
