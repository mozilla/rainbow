/* Expose API under window.navigator.service.media */
if (window && window.navigator) {
    if (!window.navigator.service)
        window.navigator.service = {};
    window.navigator.service.media = {
        beginSession: function(params, ctx, obs) {
            return _beginSession(window.location, params, ctx, obs);
        },
        beginRecord: function() {
            return _beginRecord(window.location);
        },
        pauseRecord: function() {
            return _pauseRecord(window.location);
        },
        endRecord: function() {
            return _endRecord(window.location);
        },
        endSession: function() {
            return _endSession(window.location);
        }
    }
}
