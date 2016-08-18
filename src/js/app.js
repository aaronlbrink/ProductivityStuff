+function () {
'use strict';

var Promise = require('bluebird');
var url = require('url');

var endpoint = 'http://brinkdatabase.com/sii';
var pinUri = endpoint + '/scripts/pin-gen.php';
var dataUri = endpoint + '/scripts/data-gen.php';

var always = function (thenable, action) {
  return thenable.then(function (value) {
    action();
    return value;
  }, function (ex) {
    action();
    return Promise.reject(ex);
  });
};

var AjaxError = function (xhr, status, text) {
  this.xhr = xhr;
  this.status = status;
  this.text = text;
};
AjaxError.prototype = new Error();

/**
 * See authenticatedAjax().
 */
var ajax = function (options) {
  return new Promise(function (resolve, reject) {
    options.method = options.method || 'GET';
    if (!options.uri) {
      throw new Error('options.uri must be specified');
    }

    var xhr = new XMLHttpRequest();
    xhr.addEventListener('load', function () {
      if (xhr.responseType === 'json') {
        return resolve(xhr.response);
      }
      if (typeof xhr.response === 'string') {
        try
        {
          return resolve(JSON.parse(xhr.response));
        }
        catch (ex)
        {
          return reject(ex);
        }
      }
      return reject(new Error('Unable to parse response.'));
    });
    xhr.addEventListener('error', function () {
      console.log('error: ' + xhr.status + ', ' + xhr.statusText);
      if (xhr.status === 0
        || xhr.status >= 500 && xhr.status < 600) {
        // Retry
        console.log('Got response: ' + xhr.status + '. Retrying in 8.');
        setTimeout(function () {
          ajax.then(resolve, reject);
        }, 8000);
        return;
      }
      return reject(new AjaxError(xhr, xhr.status, xhr.statusText));
    });
    xhr.open(options.method, options.uri);
    if (options.data) {
      if (options.method !== 'POST') {
        throw new Error('We only support sending data through POST.');
      }
      var formData = new FormData();
      for (var i in options.data) {
        formData.append(i, options.data[i]);
      }
    }
    xhr.send(formData);
  });
};

/**
 * \param options
 *   With keys:
 *     uri
 *     data
 * \returns
 *   Promise
 */
var authenticatedAjax = function (options) {
  return getAccessToken().then(function (access_token) {
    console.log('got access token!');
    var parsedUri = url.parse(options.uri, true);
    parsedUri.query.access_token = access_token;
    options.uri = url.format(parsedUri);
    return ajax(options).catch(function (ex) {
      if (ex instanceof AjaxError) {
        if (ex.status == 403) {
          // The API has rejected our token. Maybe it needs to be bound still?
          return bindAccessToken(access_token).then(function () {
            return authenticatedAjax(options);
          });
        }
      }
      return Promise.reject(ex);
    });
  });
};

var bindingAccessToken = null;
/**
 * Bind an access token (using the PIN workflow).
 */
var bindAccessToken = function() {
  if (bindingAccessToken) {
    return bindingAccessToken;
  }
  return bindingAccessToken = always(getAccessToken().then(function (access_token) {
    return ajax({
      uri: pinUri,
      data: {
        access_token: access_token,
      },
    });
  }).then(function (data) {
    throw new Error('dealing with data: ' + JSON.stringify(data));
  }), function () {
    bindingAccessToken = null;
  });
};

var gettingAccessToken = null;
/**
 * Get the access token.
 */
var getAccessToken = function () {
  if (!gettingAccessToken) {
    gettingAccessToken = always(new Promise(function (resolve, reject) {
      var access_token = localStorage.getItem('access_token');
      console.log('access token found in storage: ' + access_token);
      if (access_token) {
        return resolve(access_token);
      }
      // Get an access token.
      resolve(ajax({
        uri: pinUri,
      }).then(function (data) {
        // With a new access token, reset some things—including the log sequence.
        try
        {
          localStorage.setItem('access_token', data.access_token);
          localStorage.setItem('log_sequence', 0);
          gettingAccessToken = null;
          return getAccessToken();
        }
        catch (ex)
        {
          if (ex instanceof QuotaExceededError) {
            // Try clearing the storage and trying again. Yes, this
            // is to try to recover from a full log preventing us from
            // authenticating.
            localStorage.clear();
            gettingAccessToken = null;
            return getAccessToken();
          }
          throw ex;
        }
      }));
    }), function () {
      gettingAccessToken = null;
    });
  }
  return gettingAccessToken;
};

var currentUpload = null;
var upload = function () {
  if (currentUpload) {
    return currentUpload;
  }
  return currentUpload = always(authenticatedAjax({
    uri: 'asdf',
  }), function () {
    currentUpload = null;
  });
};

var currentDownload = null;
var download = function () {
  if (currentDownload) {
    return currentDownload;
  }
  return currentDownload = always(authenticatedAjax({
    uri: dataUri,
  }).then(function (data) {
    console.log('Got data! ' + JSON.stringify(data));
  }), function () {
    currentDownload = null;
  });
};

// Called when JS is ready
Pebble.addEventListener(
	"ready",
	function(e) {
		console.log('ready');
    // Let watch know it can start uploading data to us if it wants.
		Pebble.sendAppMessage({MESSAGE: 'READY'});
    // Upload any logs.
    upload().catch(function (ex) {console.log('threw');console.log(ex);});
    // Download updated data.
    download().catch(function (ex) {console.log('here');console.log(ex);});
});
												
// Called when incoming message from the Pebble is received
Pebble.addEventListener(
	"appmessage",
	function(e) {
		console.log('Received, err, something ;-): ' + JSON.stringify(e.payload));
		if (e.payload.status)
			console.log("Received Status: " + e.payload.status);
});
}();