# LTIContainerSpawner

LTIカスタムパラメータ拡張による LMS - JupyterHub 連携に関する研究．
- LMS and JupyterHub Integration by LTI Custom Parameters Extension. 

Please see bellow wiki (but sorry, this wiki is Japanese Text only)
- https://www.nsl.tuis.ac.jp/xoops/modules/xpwiki/?Moodle+JupyterHub


# 概要
- **Moodle(LMS)** から **LTI**を利用して **JupyterHub** にSSOする際に幾つかの LTIカスタムパラメータを渡し，**JupyterHub** を制御する．
- さらに JupyterHub から環境変数を使用して，コンテナ（Docker/Podman）を制御する．
- Moodle側で LTIカスタムパラメータの設定補助を行うモジュールが **mod_ltids**
- JupyterHub側で LTIカスタムパラメータを処理し，コンテナの制御を行うのが **LTIContainerSpawner**
    - LTIContainerSpawner は **LTIDockerSpawner** と **LTIPodmanSpawner** から成る．

# 機能
- 以下の機能をコース内の外部ツール（LTI設定）毎に設定可能．（同じJupyterHubホストに対して複数同時設定が可能）
    - ユーザグループのサポート．(JupyterHub + LTIContainerSpawner)
    - 教師ユーザと学生ユーザの分離．(LTIContainerSpawner)
    - ユーザ毎の柔軟な Volume のマウント，及びアクセス権（教師と学生）の設定．(mod_ltids + LTIContainerSpawner)
        - マウントする Volume を自由に指定可能．
        - 教材の配布と収集に有用．
    - ユーザの作業用スペースの設定．(mod_ltids + LTIContainerSpawner)
        - ユーザの作業用スペースへ任意の課題を自動コピー可能．
    - マウントした Volume への任意名でのアクセス．(mod_ltids + LTIContainerSpawner)
    - Volume のリモート生成と削除．(mod_ltids)
    - 起動コンテナイメージのリモート選択．コンテナイメージ名のフィルタリング．(mod_ltids)
    - 起動 URL（Lab/Notebook）の選択．(mod_ltids + LTIContainerSpawner)
    - iframe の一部サポート．(mod_ltids + LTIContainerSpawner)
        - 動くための条件がシビアなので（tornado のバージョンやWebブラウザの種類によって条件が変わる），"一部サポート" とする．
    - コンテナの使用する CPU/Momery リソースの制限．(mod_ltids)
    - コンテナとして Podman を選択可能．(LTIContainerSpawner)
- 現在構築中の機能（オプション扱い）
    - ユーザの学習状況のリアルタイムでの確認と可視化．(feserver + Moodle)
# インストール
## 必要な既知システム（細かいインストール手順は省略）
### Moodle
- v3.5 以上
    - 3.4以下は試していないだけで，LTIをサポートするパージョンであれば動く可能性は大．
- Moodle ホストはJupyterHub が動くホストとは別のホストでもOK．
- JupyterHub で使用するユーザの認証が可能である必要がある（通常は**LDAP**などを用いる）．
- 外部ツール，Webサービス（オプション）を使用する．
- Docker を使用する場合はMoodle ホスト側に，少なくとも docker-ce-cli/docker-cli がインストールされている必要がある．
- Podman を使用する場合はMoodle ホスト側に，少なくとも podman-remote がインストールされている必要がある． 
### Container システム
- **Docker** または **Podman** が使用可能（両方を一台のホストにインストールして運用するのは不可能だと思われる）
### JupyterHub
- **SystemUserSpawner**
    - コンテナとして Docker を使用する場合に必要．（Podmanを使用する場合は不要）
    - 通常は JupyterHub に付属してインストールされる．ただし最新版でない場合は，別途手動でインストールする．
### NSS（ユーザ情報）
- JupyterHub ホスト側で，使用するユーザの情報（/etc/passwd, /etc/group形式）が必要（パスワード自体の情報は不要）．
    - **getent passwd** コマンドでユーザの情報（/etc/passwd形式）が取れることが必要．（および /etc/group形式の情報も）
- Moodle ホストと同様に LDAP を用いても良いが，**altfiles** を用いても良い．
##### altfiles 
- /etc/passwd, /etc/group 以外のファイルからユーザ情報を得るNSS用モジュール．
- ファイルの設置場所はコンパイル時に指定
```
# git clone https://github.com/aperezdc/nss-altfiles.git
# cd nss-altfiles/
# ./configure --prefix=/usr --datadir=/usr/local/etc --with-type=pwd,grp
# make
# make install 
# ln -s /usr/lib/libnss_altfiles.so.2 /usr/lib64/libnss_altfiles.so
# ldconfig
```
- この場合，/usr/local/etc/ に必要な passwd, group ファイルをコピーする．
    - group ファイルはあまり変化しない筈であるし，エントリも少ないと思うので，/etc/group と統合しても良いかも知れない．
- **/etc/nsswitch.conf** ファイルで，passwd とgroup のエントリに対して，files の後ろに **altfiles** を追加する．
```:/etc/nsswitch.conf
passwd: files altfiles
group:  files altfiles
```
### LTI
- Moodle から JupyterHub にSSOするためのモジュール（Moodleの外部サービスを使用）
- V1.0.0 では Moodleで使用するに当たりパッチが必要だったが，v1.2.0 ではパッチが不必要になった．
- **oauthlib** が先にインストールされていなければならない．
```
# pip install jupyterhub-ltiauthenticator==1.2.0
```
- Moodle の外部ツールと JupyterHubの設定ファイル（jupyterhub_config.py）で，コンシューマ鍵と共有秘密鍵を合わせる必要がある．
    - 鍵は **openssl rand -hex 32** コマンドなどで生成する．
```python:jupyterhub_config.py
# 設定サンプル
# for LTI v1.2.0
c.JupyterHub.authenticator_class = 'ltiauthenticator.LTIAuthenticator'
c.LTI11Authenticator.consumers = {
   "b18e82ec683724743236fade71350720029a29144a585c66f6741d8e9c6e0d83" : "c0fe2924dbb0f4701d898d36aaf9fd89c7a3ed3a7db6f0003d0e825a7eccb41c"
}
c.LTI11Authenticator.username_key = 'ext_user_username'
```
### Culler（オプション）
- 接続の切れた docker/podman コンテナを削除する機能．
- 様々な種類がある．今回のシステムについては以下の物をお勧めする．
#### cull_idle_servers.py
- https://github.com/jupyterhub/jupyterhub/tree/a6b7e303df03865d6420f6bccdf627b39f1d0dc1/examples/cull-idle
```
# pip3 install pycurl
# wget https://raw.githubusercontent.com/jupyterhub/jupyterhub/a6b7e303df03865d6420f6bccdf627b39f1d0dc1/examples/cull-idle/cull_idle_servers.py
# cp cull_idle_servers.py /usr/local/bin
# chmod a+rx /usr/local/bin/cull_idle_servers.py
```
- JupyterHub の設定ファイル（jupyterhub_config.py）での設定（LTIDockerSpawner/LTIPodmanSpawnerではデフォルトで設定済）
```python:jupyterhub_config.py
import sys

c.JupyterHub.services = [
   {
       'name': 'idle-culler',
       'admin': True,
       'command': [
           sys.executable,
           '/usr/local/bin/cull_idle_servers.py',
           '--timeout=3600'
       ],
   }
]
```
## NSLによる拡張（今回の仕事）
### LTIContainerSpawner
- https://gitlab.nsl.tuis.ac.jp/iseki/lticontainerspawner
##### LTIDockerSpawner
```
# git clone https://gitlab.nsl.tuis.ac.jp/iseki/lticontainerspawner.git
# vi lticontainerspawner/etc/jupyterhubdocker_config.py
# jupyterhub -f lticontainerspawner/etc/jupyterhub_docker_config.py
```
##### LTIpodmanSpawner
```
# git clone https://gitlab.nsl.tuis.ac.jp/iseki/lticontainerspawner.git
# vi lticontainerspawner/etc/jupyterhub_podman_config.py
# jupyterhub -f lticontainerspawner/etc/jupyterhub_podman_config.py
```
### mod_ltids
- https://gitlab.nsl.tuis.ac.jp/iseki/mod_ltids
```
# cd [Moodle Path]/mod
# git clone https://gitlab.nsl.tuis.ac.jp/iseki/mod_ltids.git
# mv mod_ltids ltids
# chown -R [wwwサーバの実効ユーザ].[wwwサーバの実効グループ] ltids
adminユーザで Moodleにログインする
```
### feserver (オプション)
- ユーザの学習状況のデータを収集するためのツール．
- Moodle と JupyterHub に間に設置し，WebSocketのデータを解析して Moodle にXML-RPCの形でデータを投げる．
- 本来は MITM 的動作を行う私的試験用ツール（TCP中継サーバ）．
    - subversionリポジトリ： svn co http://www.nsl.tuis.ac.jp/svn/linux/feserver/trunk feserver
    - 各種モジュールを読み込むことにより，色々な通信データの処理が可能．
    - 今回は feplg_nbws.so モジュールを使用する．
    - コンパイルには JunkBox_Lib が必要
----------------
- コンパイルと起動
```comannd:コンパイルと起動
# svn co http://www.nsl.tuis.ac.jp/svn/linux/JunkBox_Lib/trunk JunkBox_Lib
# cd JunkBox_Lib
# ./config.sh
# ./configure --enable-ssl
# make
# cd ..
# svn co http://www.nsl.tuis.ac.jp/svn/linux/feserver/trunk feserver
# cd feserver
# make
# vi nbsw.conf
# ./fesvr ......  -m feplg_nbws.so  --conf nbsw.conf
```
