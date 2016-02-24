var express = require('express');
var path = require('path');
var favicon = require('serve-favicon');
var logger = require('morgan');
var cookieParser = require('cookie-parser');
var bodyParser = require('body-parser');
var fs = require('fs');

var routes = require('./routes/index');
var users = require('./routes/users');
var murmur = require('./murmur');
var crc32 = require('buffer-crc32');

var app = express();

// view engine setup
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'jade');

// uncomment after placing your favicon in /public
//app.use(favicon(path.join(__dirname, 'public', 'favicon.ico')));
app.use(logger('dev'));
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: false }));
app.use(cookieParser());
app.use(require('stylus').middleware(path.join(__dirname, 'public')));
app.use(express.static(path.join(__dirname, 'public')));

var fileMap = {

};

app.get('/files/**', function(req, res) {
  var hash = parseInt(req.header('X-Local-Version'));
  var file = req.params[0];
  if(isNaN(hash) == false) {
    var key = file.toUpperCase();
    if(fileMap[key]) {
      var entry = fileMap[key];
      if(entry.hash == hash) {
        res.sendStatus(304);
        return;
      }
    }

    fs.readFile(path.join('E:\\WoW_Wotlk', file), (function (hash, file) {
      return function (err, result) {
        var curHash = crc32.unsigned(result);
        console.log(curHash + " - " + hash);
        fileMap[file] = {
          hash: curHash
        };

        if (curHash == hash) {
          res.sendStatus(304);
        } else {
          res.send(result);
        }
      };
    })(hash, key));
  } else {
    res.sendFile(path.join('E:\\WoW_Wotlk', file));
  }
});

// catch 404 and forward to error handler
app.use(function(req, res, next) {
  var err = new Error('Not Found');
  err.status = 404;
  next(err);
});

// error handlers

// development error handler
// will print stacktrace
if (app.get('env') === 'development') {
  app.use(function(err, req, res, next) {
    res.status(err.status || 500);
    res.render('error', {
      message: err.message,
      error: err
    });
  });
}

// production error handler
// no stacktraces leaked to user
app.use(function(err, req, res, next) {
  res.status(err.status || 500);
  res.render('error', {
    message: err.message,
    error: {}
  });
});


module.exports = app;
