# git-practice

### git方法紀錄 ###
* 在git上面先創repo
  * git clone下來
  * 在裡面建立project..寫code
  * 最後git add .
  * git commit
  * git push<br>
* 已經在local有repo
  * git init
  * git add .
  * git commit
  * git remote add origin https://github.com/yourusername/your-repo-name.git
  * git push -u origin master<br>
* remove remote origin 方法
  * git remote rm origin<br>
* remind
  * 每次再更改code先git pull，確保資料最新
  
* git add skill
  * git add -A stages All
  * git add . stages new and modified, without deleted
  * git add -u stages modified and deleted, without new
  * 推薦在加入檔案時，指令下 git add -i ，也就是互動模式，進入後選2可以選擇哪些檔案要加入，3是移除已加入的檔案，4是加入untracked的檔案。完成後選1可以確認狀態，然後再下 git commit -m 

  
