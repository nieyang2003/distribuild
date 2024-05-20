```
distribuild gcc ...
```
运行过程（省略参数）
```
           main()
             |
             v
         Compile()-------------------------------+- 构造 CompilerArgs
                                                 |- 找到编译器绝对路径 --传给--> CompilerArgs
                                                 |- 检查参数判断能否继续 [quit]
       RewriteFile() ----------------------------+- 预处理源文件并压缩生成缓存键值
             |                                   |- 如果文件太小则直接本地编译 [quit]
             |-                                  |- while(true)
             |-                                  | 
                                                 | 
      CompileOnCloud() --------------------------| 
             |                                   |- 处理结果，重新提交等
             |-SubmitComileTask()                |- 写入文件
             |-WaitForCloudCompileResult()       |

```