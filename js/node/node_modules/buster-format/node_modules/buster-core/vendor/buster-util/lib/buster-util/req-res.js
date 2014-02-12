var fakeRequest = {
    on: function (event, listener) {
        this.listeners = this.listeners || {};
        this.listeners[event] = this.listeners[event] || [];
        this.listeners[event].push(listener);

        var el = this.listeners.end && this.listeners.end.length || 0;
        var dl = this.listeners.data && this.listeners.data.length || 0;

        if (el == dl && el > 0 && typeof this.body == "string") {
            this.emit(this.body);
        }

        if (typeof this.body != "string" && el > 0) {
            for (var i = 0; i < el; ++i) {
                this.listeners.end[i]();
            }
        }
    },

    emit: function (body) {
        if (typeof body != "string") {
            throw new Error("Fake request attempted to emit body '" + body + "'");
        }

        var pieces = [body.substr(0, body.length / 2),
                      body.substr(body.length / 2, body.length)];
        var listeners = this.listeners && this.listeners.data || [];

        for (var i = 0, l = listeners.length; i < l; ++i) {
            listeners[i](pieces[0]);
            listeners[i](pieces[1]);
        }

        listeners = this.listeners && this.listeners.end || [];

        for (i = 0, l = listeners.length; i < l; ++i) {
            listeners[i]();
        }
    }
};

var fakeResponse = {
    body: "",
    writeHeadCount: 0,
    endCount: 0,

    write: function (body) {
        this.body = body || "";
    },

    writeHead: function (status, headers) {
        if (this.writeHeadCount == 1) {
            throw new Error("writeHead called twice");
        }

        this.writeHeadCount = 1;
        this.status = status;
        this.headers = headers;
    },

    end: function (body) {
        if (this.endCount == 1) {
            throw new Error("end called twice");
        }

        if (typeof body != "undefined") {
            this.body = body;
        }

        this.endCount = 1;
        this.headers = this.headers || {};
        this.complete = true;
    }
};

module.exports = {
    fakeResponse: fakeResponse,
    fakeRequest: fakeRequest,

    res: function () {
        return Object.create(fakeResponse);
    },

    req: function (method, url, headers, body) {
        var req = Object.create(fakeRequest);
        req.method = method;
        req.url = url;
        req.headers = headers;
        req.body = body;

        return req;
    },

    get: function (url, headers) {
        return this.req("GET", url, headers);
    },

    post: function (url, headers, body) {
        return this.req("POST", url, headers, body);
    },

    head: function (url, headers) {
        return this.req("HEAD", url, headers);
    }
};
