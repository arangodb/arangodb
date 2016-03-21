/// Copyright (c) 2012 Ecma International.  All rights reserved. 
/// This code is governed by the BSD license found in the LICENSE file.

/* A section of the spec. Stores test results and subsections and some rolled up stats on how many tests passed or
 * failed under that section
 */
function Section(parentSection, id, name) {
    this.parentSection = parentSection;
    this.id = id;
    this.name = name;
    this.subsections = {};
    this.tests = [];
    this.totalTests = 0;
    this.totalPassed = 0;
    this.totalFailed = 0;
    this.totalFailedToLoad = 0;

    var section = this,
        RED_LIMIT = 50,
        YELLOW_LIMIT = 75,
        GREEN_LIMIT = 99.9;

    /* Get the class for a result cell given the pass percent. */
    function rollupCellClass(passPercent) {
        if(passPercent >= GREEN_LIMIT) {
            return "reportGreen";
        } else if (passPercent >= YELLOW_LIMIT) {
            return "reportLightGreen";
        } else if (passPercent >= RED_LIMIT) {
            return "reportYellow";
        }

        return "reportRed";
    }

    /* Calculate pass percent */
    this.passPercent = function() {
        if(this.totalTests === 0) {
            return 0;
        }

        return Math.round((this.totalPassed / this.totalTests) * 100);
    };

    /* Add a test result to this section. Pushes the result to the
     * test array and passes the result to addTestResult to tabulate
     * pass/fail numbers
     */
    this.addTest = function(test) {
        this.tests.push(test);
        this.addTestResult(test);
    };

    /* Increments the various rollup counters for this section and all
     * parent sections
     */
    this.addTestResult = function(test) {
        this.totalTests++;

        if(test.result === "pass")
            this.totalPassed++;
        else
            this.totalFailed++;

        if (test.error === 'Failed to load test case (probable parse error).')
            this.totalFailedToLoad++;

        if(this.parentSection !== null)
            this.parentSection.addTestResult(test);
    };

    /* Renders this section as HTML. Used for the report page.*/
    this.toHTML = function(options) {
        var defaultOptions = {header: false, renderSubsections: true};

        if (typeof options === undefined) {
            options = defaultOptions;
        } else {
            options = $.extend(defaultOptions, options);
        }

        var html = '<tbody id="section_' + this.id.replace(/\./g, "_") + '">';

        if(options.header) {
            html += "<tr><td class='tblHeader' colspan='3'>Chapter " + this.id + " - " + this.name + "</td>" +
                    "<td class='" + rollupCellClass(this.passPercent()) + "'>" + this.passPercent() + "%</td></tr>";
        }

        for(var i = 0; i < this.tests.length;i++) {
            test = this.tests[i];
            html += "<tr><td>" + test.id + "</td>" +
                    "<td>" + test.description + "</td>" +
                    "<td><a class='showSource' href='#" + test.id +
                    "'>[source]</a></td>" +
                    "<td class='" + test.result + "'>" + test.result +
                    "</td></tr>";
        }

        for(var sectionId in this.subsections) {
            var section = this.subsections[sectionId];

            if(section.totalTests > 0) {
                if(options.renderSubsections) {
                    html += section.toHTML({
                        header: true,
                        renderSubsections: false});
                } else {
                    html += "<tr><td colspan='3'><a class='section' href='#" +
                    section.id + "'>Chapter " + section.id + " - " +
                    section.name + "</a></td>" +
                            "<td class='" +
                            rollupCellClass(section.passPercent()) + "'>" +
                            section.passPercent() + "%</td></tr>";
                }
            }
        }

        return html + "</tbody>";
    };

    /* Render this section as XML. Used for the report page. */
    this.toXML = function() {
        var xml = "";
        if(this.id != 0) {
            xml += "<section id='" + this.id + "' name='" + this.name +
                   "'>\r\n";

            for (var i = 0; i < this.tests.length; i++) {
                xml += '<test>\r\n' +
                          '  <testId>' + this.tests[i].id + '</testId>\r\n' +
                          '  <res>' + this.tests[i].result + '</res>\r\n' +
                          '</test>\r\n';
            }
        }

        for (var subsection in this.subsections) {
            xml += this.subsections[subsection].toXML();
        }

        if(this.id != 0) {
            xml += '</section>\r\n';
        }

        return xml;
    };

    /* Reset counts and remove tests. */
    this.reset = function() {
        this.tests = [];
        this.totalTests = 0;
        this.totalPassed = 0;
        this.totalFailed = 0;
        this.totalFailedToLoad = 0;

        for(var subsection in this.subsections) {
            this.subsections[subsection].reset();
        }
    };
}
