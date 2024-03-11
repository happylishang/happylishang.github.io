
###  execute没有返回值
    
        public void execute(Runnable command) {
        if (command == null) {
            throw new NullPointerException();
        } else {
            int c = this.ctl.get();
            if (workerCountOf(c) < this.corePoolSize) {
                if (this.addWorker(command, true)) {
                    return;
                }

                c = this.ctl.get();
            }

            if (isRunning(c) && this.workQueue.offer(command)) {
                int recheck = this.ctl.get();
                if (!isRunning(recheck) && this.remove(command)) {
                    this.reject(command);
                } else if (workerCountOf(recheck) == 0) {
                    this.addWorker((Runnable)null, false);
                }
            } else if (!this.addWorker(command, false)) {
                this.reject(command);
            }

        }
    }
    
### submit有返回值 ，最终submit还是调用execute
    
    public Future<?> submit(Runnable task) {
        if (task == null) {
            throw new NullPointerException();
        } else {
            RunnableFuture<Void> ftask = this.newTaskFor(task, (Object)null);
            this.execute(ftask);
            return ftask;
        }
    }
    
 
    