mymodule = require "mymodule"
sqlite3 = require "lsqlite3"

-- /.init.lua is loaded at startup in redbean's main process
HidePath('/usr/share/zoneinfo/')
HidePath('/usr/share/ssl/')

-- open a browser tab using explorer/open/xdg-open
-- LaunchBrowser('/tool/net/demo/index.html')

-- sql database (see sql.lua)
db = sqlite3.open_memory()
db:exec[[
  CREATE TABLE test (
    id INTEGER PRIMARY KEY,
    content TEXT
  );
  INSERT INTO test (content) VALUES ('Hello World');
  INSERT INTO test (content) VALUES ('Hello Lua');
  INSERT INTO test (content) VALUES ('Hello Sqlite3');
]]

function OnServerListen(fd, ip, port)
   unix.setsockopt(fd, unix.SOL_TCP, unix.TCP_SAVE_SYN, true)
   return false
end

function OnClientConnection(ip, port, serverip, serverport)
   syn, synerr = unix.getsockopt(GetClientFd(), unix.SOL_TCP, unix.TCP_SAVED_SYN)
end

-- this intercepts all requests if it's defined
function OnHttpRequest()
   Log(kLogInfo, "client is running %s and reports %s" % {
          finger.GetSynFingerOs(finger.FingerSyn(syn)),
          GetHeader('User-Agent')})
   if HasParam('magic') then
      Write('<p>\r\n')
      Write('OnHttpRequest() has intercepted your request<br>\r\n')
      Write('because you specified the magic parameter\r\n')
      Write('<pre>\r\n')
      Write(EscapeHtml(LoadAsset('/.init.lua')))
      Write('</pre>\r\n')
   else
      Route() -- this asks redbean to do the default thing
   end
   SetHeader('Server', 'redbean!')
end

function Adder(x, y)
   return x + y
end
