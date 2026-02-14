/**
 * ============================================
 * Motorik GUI Framework - JavaScript Controller
 * Modular, reusable control system for JUCE webview
 * ============================================
 */

'use strict';

/* ============================================
   COLOR UTILITY
   Gets CSS custom property values from :root
   ============================================ */
function getCSSColor(varName) {
    return getComputedStyle(document.documentElement).getPropertyValue(varName).trim();
}

// Color constants - populated from CSS variables
var COLORS = {
    bgPrimary: getCSSColor('--color-bg-primary'),
    bgSecondary: getCSSColor('--color-bg-secondary'),
    bgControl: getCSSColor('--color-bg-control'),
    bgHover: getCSSColor('--color-bg-hover'),
    border: getCSSColor('--color-border'),
    textPrimary: getCSSColor('--color-text-primary'),
    textSecondary: getCSSColor('--color-text-secondary'),
    accent: getCSSColor('--color-accent'),
    rowBD: getCSSColor('--color-row-bd'),
    rowBDAlt: getCSSColor('--color-row-bd-alt'),
    rowSN: getCSSColor('--color-row-sn'),
    rowSNAlt: getCSSColor('--color-row-sn-alt'),
    rowHH: getCSSColor('--color-row-hh'),
    rowHHAlt: getCSSColor('--color-row-hh-alt')
};

function MotorikGUI() {
    this.controls = {};
    this.setupResponsiveSizing();
}

/* ============================================
   RESPONSIVE SIZING SYSTEM
   ============================================ */

MotorikGUI.prototype.calculateKnobSize = function() {
    var windowWidth = window.innerWidth;
    var windowHeight = window.innerHeight;
    var minDimension = Math.min(windowWidth, windowHeight);

    // Size ranges
    var minSize = 30;
    var maxSize = 150;

    // Window size thresholds
    var minWindow = 600;
    var maxWindow = 1200;

    var sizeRatio;
    if (minDimension <= minWindow) {
        sizeRatio = 0;
    } else if (minDimension >= maxWindow) {
        sizeRatio = 1;
    } else {
        sizeRatio = (minDimension - minWindow) / (maxWindow - minWindow);
    }

    // Smoothstep easing
    var easedRatio = sizeRatio * sizeRatio * (3 - 2 * sizeRatio);

    return Math.round(minSize + easedRatio * (maxSize - minSize));
};

MotorikGUI.prototype.updateResponsiveKnobSizes = function() {
    var newSize = this.calculateKnobSize();
    document.documentElement.style.setProperty('--knob-size-responsive', newSize + 'px');
};

MotorikGUI.prototype.setupResponsiveSizing = function() {
    var that = this;

    this.updateResponsiveKnobSizes();

    var resizeTimeout;
    window.addEventListener('resize', function() {
        clearTimeout(resizeTimeout);
        resizeTimeout = setTimeout(function() {
            that.updateResponsiveKnobSizes();
        }, 16);
    });
};

/* ============================================
   ROTARY KNOB CONTROL
   ============================================ */

/**
 * Creates a rotary knob control
 * @param {HTMLElement} container - DOM element to attach knob to
 * @param {Object} config - Configuration object
 * @param {string} config.id - Unique identifier for the knob
 * @param {number} [config.size=80] - SVG size in pixels
 * @param {number} [config.min=0] - Minimum value
 * @param {number} [config.max=127] - Maximum value
 * @param {boolean} [config.bipolar=false] - Whether knob is bipolar (-1 to 1)
 * @param {number} [config.value] - Initial value
 * @param {Function} [config.onChange] - Callback when value changes
 * @returns {Object|null} Control object with updateDisplay, setDisabled methods, or null on error
 */
MotorikGUI.prototype.createKnob = function(container, config) {
    if (!container) {
        console.error('MotorikGUI.createKnob: container is required');
        return null;
    }

    if (!config || !config.id) {
        console.error('MotorikGUI.createKnob: config.id is required');
        return null;
    }

    var id = config.id;
    var size = config.size || 80;
    var min = config.min !== undefined ? config.min : 0;
    var max = config.max !== undefined ? config.max : 127;
    var isBipolar = config.bipolar || false;
    var initialValue = config.value !== undefined ? config.value : (isBipolar ? 0 : 0);
    var onChange = config.onChange || null;

    // Create SVG
    var svgNS = 'http://www.w3.org/2000/svg';
    var svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('width', size);
    svg.setAttribute('height', size);
    svg.setAttribute('viewBox', '0 0 150 150');
    svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');

    // Create knob circle
    var circle = document.createElementNS(svgNS, 'circle');
    circle.setAttribute('cx', '75');
    circle.setAttribute('cy', '75');
    circle.setAttribute('r', '60');
    circle.setAttribute('fill', COLORS.bgControl);
    circle.setAttribute('stroke', COLORS.border);
    circle.setAttribute('stroke-width', '0');
    circle.setAttribute('filter', 'url(#knobShadow)');
    svg.appendChild(circle);

    var centerX = 75;
    var centerY = 75;
    var radius = 60;

    // Angle system: -135° to +135° (270° total range)
    var minAngle = -135;
    var maxAngle = 135;
    var range = maxAngle - minAngle;
    var centerAngle = 0;
    var halfRange = range / 2.0;

    var control = {
        element: svg,
        container: container,
        value: initialValue,
        defaultValue: initialValue,  // Store default value for double-click reset
        min: min,
        max: max,
        isBipolar: isBipolar,
        size: size,
        steps: 128,
        isDragging: false,
        onChange: onChange,
        arcs: {},
        pointer: null
    };

    // Create arcs and pointer based on type
    if (isBipolar) {
        // Negative arc (for values < 0)
        var negArc = document.createElementNS(svgNS, 'path');
        negArc.setAttribute('fill', 'none');
        negArc.setAttribute('stroke', COLORS.accent);
        negArc.setAttribute('stroke-width', '2');
        negArc.setAttribute('stroke-linecap', 'round');
        negArc.setAttribute('vector-effect', 'non-scaling-stroke');
        svg.appendChild(negArc);
        control.arcs.negative = negArc;

        // Positive arc (for values > 0)
        var posArc = document.createElementNS(svgNS, 'path');
        posArc.setAttribute('fill', 'none');
        posArc.setAttribute('stroke', COLORS.accent);
        posArc.setAttribute('stroke-width', '2');
        posArc.setAttribute('stroke-linecap', 'round');
        posArc.setAttribute('vector-effect', 'non-scaling-stroke');
        svg.appendChild(posArc);
        control.arcs.positive = posArc;

        // Pointer
        var pointer = document.createElementNS(svgNS, 'line');
        pointer.setAttribute('x1', '75');
        pointer.setAttribute('y1', '75');
        pointer.setAttribute('x2', '75');
        pointer.setAttribute('y2', '15');
        pointer.setAttribute('stroke', COLORS.accent);
        pointer.setAttribute('stroke-width', '2');
        pointer.setAttribute('stroke-linecap', 'round');
        pointer.setAttribute('vector-effect', 'non-scaling-stroke');
        svg.appendChild(pointer);
        control.pointer = pointer;
    } else {
        // Unipolar arc
        var arc = document.createElementNS(svgNS, 'path');
        arc.setAttribute('fill', 'none');
        arc.setAttribute('stroke', COLORS.accent);
        arc.setAttribute('stroke-width', '2');
        arc.setAttribute('stroke-linecap', 'square');
        arc.setAttribute('vector-effect', 'non-scaling-stroke');
        svg.appendChild(arc);
        control.arcs.unipolar = arc;

        // Pointer
        var pointer = document.createElementNS(svgNS, 'line');
        pointer.setAttribute('x1', '75');
        pointer.setAttribute('y1', '75');
        pointer.setAttribute('x2', '75');
        pointer.setAttribute('y2', '15');
        pointer.setAttribute('stroke', COLORS.accent);
        pointer.setAttribute('stroke-width', '2');
        pointer.setAttribute('stroke-linecap', 'round');
        pointer.setAttribute('vector-effect', 'non-scaling-stroke');
        svg.appendChild(pointer);
        control.pointer = pointer;
    }

    container.appendChild(svg);

    // Helper functions
    var arcD = function(angleA, angleB) {
        var sa = (angleA - 90) * Math.PI / 180;
        var ea = (angleB - 90) * Math.PI / 180;
        var r = radius + 2;
        var sx = centerX + Math.cos(sa) * r;
        var sy = centerY + Math.sin(sa) * r;
        var ex = centerX + Math.cos(ea) * r;
        var ey = centerY + Math.sin(ea) * r;
        var large = Math.abs(angleB - angleA) > 180 ? 1 : 0;
        return 'M ' + sx + ' ' + sy + ' A ' + r + ' ' + r + ' 0 ' + large + ' 1 ' + ex + ' ' + ey;
    };

    var angleForStep = function(step) {
        var t = step / (control.steps - 1);
        return minAngle + range * t;
    };

    control.updateDisplay = function() {
        if (control.isBipolar) {
            var centerStep = (control.steps - 1) / 2.0;
            var valueStep = ((control.value + 1) / 2) * (control.steps - 1);
            var offset = valueStep - centerStep;
            var angle = angleForStep(valueStep);

            control.pointer.setAttribute('transform', 'rotate(' + angle + ' 75 75)');

            // Positive arc
            if (offset > 0.0001) {
                var posT = offset / (control.steps - 1 - centerStep);
                var posAngle = centerAngle + posT * halfRange;
                control.arcs.positive.setAttribute('d', arcD(centerAngle, posAngle));
            } else {
                control.arcs.positive.setAttribute('d', '');
            }

            // Negative arc
            if (offset < -0.0001) {
                var negT = (-offset) / centerStep;
                var negAngle = centerAngle - negT * halfRange;
                control.arcs.negative.setAttribute('d', arcD(negAngle, centerAngle));
            } else {
                control.arcs.negative.setAttribute('d', '');
            }
        } else {
            var step = control.value;
            var angle = angleForStep(step);

            control.pointer.setAttribute('transform', 'rotate(' + angle + ' 75 75)');
            control.arcs.unipolar.setAttribute('d', arcD(minAngle, angle));
        }
    };

    // Disabled state
    control.disabled = false;
    control.setDisabled = function(disabled) {
        control.disabled = disabled;

        if (disabled) {
            // Add disabled class to container (for gray color only)
            container.classList.add('knob-disabled');
        } else {
            // Remove disabled class
            container.classList.remove('knob-disabled');
        }
    };

    // Mouse interaction
    svg.addEventListener('mousedown', function(e) {
        control.isDragging = true;
        var startY = e.clientY;
        var startValue = control.value;

        function onMouseMove(ev) {
            if (!control.isDragging) return;
            var delta = startY - ev.clientY;
            var newValue;

            if (control.isBipolar) {
                newValue = startValue + delta * 0.01;
                newValue = Math.max(-1, Math.min(1, newValue));
            } else {
                newValue = startValue + delta * 0.5;
                newValue = Math.max(0, Math.min(127, newValue));
            }

            control.value = newValue;
            control.updateDisplay();

            if (control.onChange) {
                control.onChange(newValue);
            }
        }

        function onMouseUp() {
            control.isDragging = false;
            document.removeEventListener('mousemove', onMouseMove);
            document.removeEventListener('mouseup', onMouseUp);
        }

        document.addEventListener('mousemove', onMouseMove);
        document.addEventListener('mouseup', onMouseUp);
        e.preventDefault();
    });

    // Double-click to reset to default value
    svg.addEventListener('dblclick', function() {
        control.value = control.defaultValue;
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   VERTICAL SLIDER CONTROL
   ============================================ */

/**
 * Creates a vertical slider control
 * @param {HTMLElement} container - DOM element to attach slider to
 * @param {Object} config - Configuration object
 * @param {string} config.id - Unique identifier
 * @param {number} [config.min=0] - Minimum value
 * @param {number} [config.max=127] - Maximum value
 * @param {number} [config.value=0] - Initial value
 * @param {Function} [config.onChange] - Callback when value changes
 * @returns {Object|null} Control object or null on error
 */
MotorikGUI.prototype.createSlider = function(container, config) {
    var id = config.id;
    var min = config.min !== undefined ? config.min : 0;
    var max = config.max !== undefined ? config.max : 127;
    var initialValue = config.value !== undefined ? config.value : 0;
    var onChange = config.onChange || null;

    var svgNS = 'http://www.w3.org/2000/svg';
    var svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('width', '20');
    svg.setAttribute('height', '120');

    // Track
    var track = document.createElementNS(svgNS, 'rect');
    track.setAttribute('x', '8');
    track.setAttribute('y', '10');
    track.setAttribute('width', '4');
    track.setAttribute('height', '100');
    track.setAttribute('fill', '#333');
    track.setAttribute('rx', '2');
    svg.appendChild(track);

    // Fill
    var fill = document.createElementNS(svgNS, 'rect');
    fill.setAttribute('x', '8');
    fill.setAttribute('y', '10');
    fill.setAttribute('width', '4');
    fill.setAttribute('height', '0');
    fill.setAttribute('fill', COLORS.accent);
    fill.setAttribute('rx', '2');
    svg.appendChild(fill);

    // Handle
    var handle = document.createElementNS(svgNS, 'rect');
    handle.setAttribute('x', '4');
    handle.setAttribute('y', '106');
    handle.setAttribute('width', '12');
    handle.setAttribute('height', '8');
    handle.setAttribute('fill', '#222');
    handle.setAttribute('stroke', COLORS.accent);
    handle.setAttribute('stroke-width', '2');
    handle.setAttribute('rx', '2');
    svg.appendChild(handle);

    container.appendChild(svg);

    var control = {
        element: svg,
        container: container,
        value: initialValue,
        min: min,
        max: max,
        totalSteps: 128,
        onChange: onChange,
        fill: fill,
        handle: handle
    };

    control.updateDisplay = function() {
        var normalized = control.value / control.max;
        var fillHeight = normalized * 100;
        control.fill.setAttribute('height', fillHeight);
        control.fill.setAttribute('y', 110 - fillHeight);
        var handleY = 106 - (normalized * 96);
        control.handle.setAttribute('y', handleY);
    };

    svg.addEventListener('pointerdown', function(e) {
        e.preventDefault();

        function onMove(ev) {
            var rect = svg.getBoundingClientRect();
            var y = ev.clientY - rect.top;
            var usable = 100;
            var t = Math.max(0, Math.min(1, 1 - ((y - 10) / usable)));
            control.value = Math.round(t * (control.totalSteps - 1));
            control.updateDisplay();

            if (control.onChange) {
                control.onChange(control.value);
            }
        }

        onMove(e);

        var mv = function(ev) { onMove(ev); };
        var up = function() {
            document.removeEventListener('pointermove', mv);
            document.removeEventListener('pointerup', up);
        };

        document.addEventListener('pointermove', mv);
        document.addEventListener('pointerup', up);
    });

    svg.addEventListener('wheel', function(ev) {
        ev.preventDefault();
        var dir = ev.deltaY > 0 ? -1 : 1;
        control.value = Math.max(control.min, Math.min(control.max, control.value + dir));
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   HORIZONTAL SLIDER CONTROL
   ============================================ */

MotorikGUI.prototype.createHorizontalSlider = function(container, config) {
    var id = config.id;
    var min = config.min !== undefined ? config.min : 0;
    var max = config.max !== undefined ? config.max : 127;
    var initialValue = config.value !== undefined ? config.value : 0;
    var onChange = config.onChange || null;

    var svgNS = 'http://www.w3.org/2000/svg';
    var svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('width', '300');
    svg.setAttribute('height', '26');

    // Track - extends to outer edges of handle circle (radius 11)
    var track = document.createElementNS(svgNS, 'rect');
    track.setAttribute('x', '3');
    track.setAttribute('y', '12');
    track.setAttribute('width', '294');
    track.setAttribute('height', '2');
    track.setAttribute('fill', COLORS.border);
    track.setAttribute('rx', '1');
    svg.appendChild(track);

    // Fill
    var fill = document.createElementNS(svgNS, 'rect');
    fill.setAttribute('x', '3');
    fill.setAttribute('y', '12');
    fill.setAttribute('width', '0');
    fill.setAttribute('height', '2');
    fill.setAttribute('fill', COLORS.accent);
    fill.setAttribute('rx', '1');
    svg.appendChild(fill);

    // Handle (circular like radio button)
    var handle = document.createElementNS(svgNS, 'circle');
    handle.setAttribute('id', 'hSliderHandle');
    handle.setAttribute('cx', '14');
    handle.setAttribute('cy', '13');
    handle.setAttribute('r', '11');
    handle.setAttribute('fill', COLORS.accent);
    handle.setAttribute('stroke', COLORS.accent);
    handle.setAttribute('stroke-width', '2');
    svg.appendChild(handle);

    container.appendChild(svg);

    var control = {
        element: svg,
        container: container,
        value: initialValue,
        min: min,
        max: max,
        totalSteps: 128,
        onChange: onChange,
        fill: fill,
        handle: handle
    };

    control.updateDisplay = function() {
        var normalized = control.value / control.max;
        var fillWidth = normalized * 272 + 11; // Extends to center of handle
        control.fill.setAttribute('width', fillWidth);
        var handleCx = 14 + (normalized * 272);
        control.handle.setAttribute('cx', handleCx);
    };

    svg.addEventListener('pointerdown', function(e) {
        e.preventDefault();

        function onMove(ev) {
            var rect = svg.getBoundingClientRect();
            var x = ev.clientX - rect.left;
            var usable = 272;
            var t = Math.max(0, Math.min(1, (x - 14) / usable));
            control.value = Math.round(t * (control.totalSteps - 1));
            control.updateDisplay();

            if (control.onChange) {
                control.onChange(control.value);
            }
        }

        onMove(e);

        var mv = function(ev) { onMove(ev); };
        var up = function() {
            document.removeEventListener('pointermove', mv);
            document.removeEventListener('pointerup', up);
        };

        document.addEventListener('pointermove', mv);
        document.addEventListener('pointerup', up);
    });

    svg.addEventListener('wheel', function(ev) {
        ev.preventDefault();
        var dir = ev.deltaY > 0 ? -1 : 1;
        control.value = Math.max(control.min, Math.min(control.max, control.value + dir));
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   TOGGLE SLIDER CONTROL (2-position flip switch)
   ============================================ */

MotorikGUI.prototype.createToggleSlider = function(container, config) {
    var id = config.id;
    var initialValue = config.value !== undefined ? config.value : false;
    var onChange = config.onChange || null;

    var svgNS = 'http://www.w3.org/2000/svg';
    var svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('width', '50');
    svg.setAttribute('height', '26');

    // Track line - extends to outer edges of handle circle (radius 11)
    var track = document.createElementNS(svgNS, 'line');
    track.setAttribute('x1', '3');
    track.setAttribute('y1', '13');
    track.setAttribute('x2', '47');
    track.setAttribute('y2', '13');
    track.setAttribute('stroke', COLORS.accent);
    track.setAttribute('stroke-width', '2');
    track.setAttribute('stroke-linecap', 'round');
    svg.appendChild(track);

    // Handle group for transform animation
    var handleGroup = document.createElementNS(svgNS, 'g');
    handleGroup.style.transition = 'transform 0.1s ease';

    // Handle (empty circle with stroke, like LEDs)
    var handle = document.createElementNS(svgNS, 'circle');
    handle.setAttribute('cx', '14');
    handle.setAttribute('cy', '13');
    handle.setAttribute('r', '11');
    handle.setAttribute('fill', COLORS.bgPrimary); // Background color fill
    handle.setAttribute('stroke', COLORS.accent);
    handle.setAttribute('stroke-width', '2');
    handleGroup.appendChild(handle);
    svg.appendChild(handleGroup);

    container.appendChild(svg);

    var control = {
        element: svg,
        container: container,
        value: initialValue,
        onChange: onChange,
        handle: handle,
        handleGroup: handleGroup,
        track: track
    };

    control.updateDisplay = function() {
        var translateX = control.value ? 22 : 0;
        control.handleGroup.style.transform = 'translateX(' + translateX + 'px)';
    };

    svg.addEventListener('pointerdown', function(e) {
        e.preventDefault();
        control.value = !control.value;
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   TOGGLE BUTTON CONTROL
   ============================================ */

MotorikGUI.prototype.createToggleButton = function(element, config) {
    var id = config.id;
    var initialValue = config.value !== undefined ? config.value : false;
    var onChange = config.onChange || null;

    var control = {
        element: element,
        value: initialValue,
        onChange: onChange
    };

    control.updateDisplay = function() {
        control.element.textContent = control.value ? 'ON' : 'OFF';
        if (control.value) {
            control.element.classList.add('active');
        } else {
            control.element.classList.remove('active');
        }
    };

    element.addEventListener('pointerdown', function(e) {
        e.preventDefault();
        control.value = !control.value;
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   MOMENTARY BUTTON CONTROL
   ============================================ */

MotorikGUI.prototype.createMomentaryButton = function(element, config) {
    var id = config.id;
    var onChange = config.onChange || null;

    var control = {
        element: element,
        value: false,
        onChange: onChange
    };

    function down() {
        control.value = true;
        control.element.classList.add('active');

        if (control.onChange) {
            control.onChange(true);
        }
    }

    function up() {
        control.value = false;
        control.element.classList.remove('active');

        if (control.onChange) {
            control.onChange(false);
        }
    }

    element.addEventListener('pointerdown', function(e) {
        e.preventDefault();
        down();
    });

    document.addEventListener('pointerup', up);
    element.addEventListener('pointerleave', up);

    return control;
};

/* ============================================
   XY PAD CONTROL
   ============================================ */

MotorikGUI.prototype.createXYPad = function(container, config) {
    var id = config.id;
    var initialX = config.x !== undefined ? config.x : 64;
    var initialY = config.y !== undefined ? config.y : 64;
    var onChange = config.onChange || null;

    var handle = container.querySelector('.xy-pad-handle');

    var control = {
        element: container,
        handle: handle,
        x: initialX,
        y: initialY,
        onChange: onChange
    };

    control.updateDisplay = function() {
        var rect = container.getBoundingClientRect();
        var handleX = (control.x / 127) * (rect.width - 16) + 8;
        var handleY = ((127 - control.y) / 127) * (rect.height - 16) + 8;
        control.handle.style.left = handleX + 'px';
        control.handle.style.top = handleY + 'px';
    };

    container.addEventListener('pointerdown', function(e) {
        e.preventDefault();

        function onMove(ev) {
            var rect = container.getBoundingClientRect();
            var newX = Math.round(((ev.clientX - rect.left - 8) / (rect.width - 16)) * 127);
            var newY = Math.round((1 - (ev.clientY - rect.top - 8) / (rect.height - 16)) * 127);
            control.x = Math.max(0, Math.min(127, newX));
            control.y = Math.max(0, Math.min(127, newY));
            control.updateDisplay();

            if (control.onChange) {
                control.onChange(control.x, control.y);
            }
        }

        onMove(e);

        var mv = function(ev) { onMove(ev); };
        var up = function() {
            document.removeEventListener('pointermove', mv);
            document.removeEventListener('pointerup', up);
        };

        document.addEventListener('pointermove', mv);
        document.addEventListener('pointerup', up);
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   DROPDOWN CONTROL
   ============================================ */

MotorikGUI.prototype.createDropdown = function(container, config) {
    var id = config.id;
    var options = config.options || ['Option 1', 'Option 2', 'Option 3', 'Option 4'];
    var initialValue = config.value !== undefined ? config.value : 0;
    var onChange = config.onChange || null;

    var textElement = container.querySelector('.dropdown-text');
    var menu = container.querySelector('.dropdown-menu');
    var items = menu.querySelectorAll('.dropdown-item');

    var control = {
        element: container,
        textElement: textElement,
        menu: menu,
        value: initialValue,
        options: options,
        onChange: onChange
    };

    control.updateDisplay = function() {
        control.textElement.textContent = control.options[control.value];
    };

    container.addEventListener('click', function(e) {
        menu.style.display = menu.style.display === 'block' ? 'none' : 'block';
        e.stopPropagation();
    });

    menu.addEventListener('click', function(e) {
        if (e.target.classList.contains('dropdown-item')) {
            var itemsArray = Array.prototype.slice.call(items);
            control.value = itemsArray.indexOf(e.target);
            control.updateDisplay();
            menu.style.display = 'none';

            if (control.onChange) {
                control.onChange(control.value);
            }

            e.stopPropagation();
        }
    });

    document.addEventListener('click', function() {
        menu.style.display = 'none';
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   SCROLLABLE LIST CONTROL
   ============================================ */

MotorikGUI.prototype.createScrollList = function(container, config) {
    var id = config.id;
    var options = config.options || ['Option 1', 'Option 2', 'Option 3', 'Option 4'];
    var initialValue = config.value !== undefined ? config.value : 0;
    var onChange = config.onChange || null;

    var textElement = container.querySelector('.scroll-list-text');

    var control = {
        element: container,
        textElement: textElement,
        value: initialValue,
        defaultValue: initialValue,  // Store default value for double-click reset
        options: options,
        onChange: onChange,
        isDragging: false,
        startY: 0,
        startValue: 0
    };

    control.updateDisplay = function() {
        control.textElement.textContent = control.options[control.value];
    };

    control.setValue = function(newValue) {
        control.value = Math.max(0, Math.min(control.options.length - 1, newValue));
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    };

    container.addEventListener('pointerdown', function(e) {
        e.preventDefault();
        control.isDragging = true;
        control.startY = e.clientY;
        control.startValue = control.value;
        container.classList.add('active');
        container.setPointerCapture(e.pointerId);
    });

    container.addEventListener('pointermove', function(e) {
        if (!control.isDragging) return;

        var deltaY = control.startY - e.clientY;
        var sensitivity = 30; // pixels per item
        var itemsToMove = Math.round(deltaY / sensitivity);
        var newValue = control.startValue + itemsToMove;

        if (newValue !== control.value) {
            control.setValue(newValue);
        }
    });

    container.addEventListener('pointerup', function(e) {
        if (control.isDragging) {
            control.isDragging = false;
            container.classList.remove('active');
            container.releasePointerCapture(e.pointerId);
        }
    });

    container.addEventListener('pointercancel', function(e) {
        control.isDragging = false;
        container.classList.remove('active');
    });

    container.addEventListener('pointerleave', function(e) {
        if (control.isDragging) {
            control.isDragging = false;
            container.classList.remove('active');
            try {
                container.releasePointerCapture(e.pointerId);
            } catch (err) {
                // Ignore errors if pointer capture was already released
            }
        }
    });

    // Mouse wheel support
    container.addEventListener('wheel', function(e) {
        e.preventDefault();
        // Only respond to significant wheel movements (threshold to avoid accidental triggers)
        if (Math.abs(e.deltaY) > 50) {
            var direction = e.deltaY > 0 ? 1 : -1;
            control.setValue(control.value + direction);
        }
    });

    // Double-click to reset to default value
    container.addEventListener('dblclick', function(e) {
        e.preventDefault();
        control.setValue(control.defaultValue);
    });

    control.updateDisplay();

    return control;
};

/* ============================================
   INITIALIZATION - AUTO-DETECT CONTROLS
   ============================================ */

/**
 * Initializes all controls in the DOM based on data attributes
 * This method scans the document for elements with control data attributes
 * and creates the corresponding control instances
 * Performance optimized: caches all value elements before processing controls
 */
MotorikGUI.prototype.initializeAllControls = function() {
    var that = this;

    // Cache all value elements first for better performance
    var valueElements = {};
    var allValueElements = document.querySelectorAll('[data-value-id]');
    for (var i = 0; i < allValueElements.length; i++) {
        var el = allValueElements[i];
        valueElements[el.getAttribute('data-value-id')] = el;
    }

    // Initialize all knobs
    var knobContainers = document.querySelectorAll('[data-knob-id]');
    for (var i = 0; i < knobContainers.length; i++) {
        (function() {
            var container = knobContainers[i];
            var id = container.getAttribute('data-knob-id');
            var size = parseInt(container.getAttribute('data-size')) || null;
            var valueElement = valueElements[id];

            // Determine if responsive (no fixed size)
            if (!size) {
                size = 80; // Default for responsive
            }

            // Check if bipolar based on ID naming convention
            // Bipolar knobs: probability (Blend), instrument-vel-2n, instrument-vel-4n, instrument-vel-4nt, instrument-vel-8n, shift, swing
            var isBipolar = (id.indexOf('-probability') !== -1 ||
                           id.indexOf('instrument-vel-2n') !== -1 ||
                           id.indexOf('instrument-vel-4n') !== -1 ||
                           id.indexOf('instrument-vel-4nt') !== -1 ||
                           id.indexOf('instrument-vel-8n') !== -1 ||
                           id.indexOf('-shift') !== -1 ||
                           id.indexOf('-swing') !== -1);

            // Set initial value for P1 and P2 knobs to maximum (127)
            var initialValue = undefined;
            if (id.indexOf('-p1') !== -1 || id.indexOf('-p2') !== -1) {
                initialValue = 127;
            }
            // Set initial value for Blend knob (probability bipolar) to minimum (-1)
            if (id.indexOf('-probability') !== -1 && isBipolar) {
                initialValue = -1;
            }
            // Set initial value for vel-scale and vel-level knobs to 100% (127)
            if (id.indexOf('-vel-scale') !== -1 || id.indexOf('-vel-level') !== -1) {
                initialValue = 127;
            }
            // Set initial value for shift knobs (bipolar) to center (0)
            if (id.indexOf('-shift') !== -1 && isBipolar) {
                initialValue = 0;
            }
            // Set initial value for swing knobs (bipolar) to center (0)
            if (id.indexOf('-swing') !== -1 && isBipolar) {
                initialValue = 0;
            }

            var control = that.createKnob(container, {
                id: id,
                size: size,
                bipolar: isBipolar,
                value: initialValue,
                onChange: function(value) {
                    if (valueElement) {
                        if (this.isBipolar) {
                            valueElement.textContent = value.toFixed(2);
                        } else {
                            valueElement.textContent = Math.round(value);
                        }
                    }
                }
            });

            that.controls[id] = control;

            // Initial value display update
            if (valueElement) {
                if (control.isBipolar) {
                    valueElement.textContent = control.value.toFixed(2);
                } else {
                    valueElement.textContent = Math.round(control.value);
                }
            }
        })();
    }

    // Initialize all sliders
    var sliderContainers = document.querySelectorAll('[data-slider-id]');
    for (var i = 0; i < sliderContainers.length; i++) {
        (function() {
            var container = sliderContainers[i];
            var id = container.getAttribute('data-slider-id');
            var valueElement = valueElements[id];

            var control = that.createSlider(container, {
                id: id,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = Math.round(value);
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = Math.round(control.value);
            }
        })();
    }

    // Initialize all horizontal sliders
    var hSliderContainers = document.querySelectorAll('[data-h-slider-id]');
    for (var i = 0; i < hSliderContainers.length; i++) {
        (function() {
            var container = hSliderContainers[i];
            var id = container.getAttribute('data-h-slider-id');
            var valueElement = valueElements[id];

            var control = that.createHorizontalSlider(container, {
                id: id,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = Math.round(value);
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = Math.round(control.value);
            }
        })();
    }

    // Initialize all toggle sliders
    var toggleSliders = document.querySelectorAll('[data-toggle-slider-id]');
    for (var i = 0; i < toggleSliders.length; i++) {
        (function() {
            var container = toggleSliders[i];
            var id = container.getAttribute('data-toggle-slider-id');
            var valueElement = valueElements[id];

            // Check if this is a "Fixed" slider and find corresponding Scale and Level knobs
            var scaleKnobControl = null;
            var levelKnobControl = null;
            var scaleLabel = null;
            if (id.indexOf('vel-fixed') !== -1) {
                // Extract row name from id (e.g., "bd-vel-fixed" -> "bd", "bdacc-vel-fixed" -> "bdacc")
                var rowName = id.replace('-vel-fixed', '');

                // Find the Scale and Level knob controls
                var scaleKnobId = rowName + '-vel-scale';
                var levelKnobId = rowName + '-vel-level';

                // Store references to the knob controls
                scaleKnobControl = scaleKnobId;
                levelKnobControl = levelKnobId;

                // Find the Scale label
                var scaleKnob = document.querySelector('[data-knob-id="' + scaleKnobId + '"]');
                if (scaleKnob) {
                    var parent = scaleKnob.parentElement;
                    scaleLabel = parent.querySelector('.control-label');
                }
            }

            var control = that.createToggleSlider(container, {
                id: id,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = value;
                    }

                    // Handle Scale/Level knobs enable/disable when Fixed is toggled
                    if (scaleKnobControl && levelKnobControl) {
                        var scaleControl = that.controls[scaleKnobControl];
                        var levelControl = that.controls[levelKnobControl];

                        if (scaleControl && scaleControl.setDisabled) {
                            // When Fixed is ON (right): disable Scale, enable Level
                            // When Fixed is OFF (left): enable Scale, disable Level
                            scaleControl.setDisabled(value);
                        }

                        if (levelControl && levelControl.setDisabled) {
                            levelControl.setDisabled(!value);
                        }

                        // Change Scale label to Fixed when Fixed is ON
                        if (scaleLabel) {
                            scaleLabel.textContent = value ? 'Fixed' : 'Scale';
                        }
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = control.value;
            }

            // Store reference for later initialization (after all knobs are created)
            if (scaleKnobControl && levelKnobControl) {
                control._needsInitialState = true;
                control._scaleKnobControl = scaleKnobControl;
                control._levelKnobControl = levelKnobControl;
            }
        })();
    }

    // Initialize all toggle buttons
    var toggleButtons = document.querySelectorAll('[data-toggle-id]');
    for (var i = 0; i < toggleButtons.length; i++) {
        (function() {
            var element = toggleButtons[i];
            var id = element.getAttribute('data-toggle-id');
            var valueElement = valueElements[id];

            // Skip vel-fixed buttons as they are now toggle sliders
            if (id.indexOf('vel-fixed') !== -1) {
                return;
            }

            // Set initial value for Bender buttons to ON (true)
            var initialValue = undefined;
            if (id.indexOf('bender') !== -1) {
                initialValue = true;
            }

            var control = that.createToggleButton(element, {
                id: id,
                value: initialValue,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = value;
                    }

                    // Show "DAW" or BPM value when Sync is toggled
                    if (id === 'midi-sync') {
                        var bpmScrollList = document.querySelector('[data-scroll-list-id="midi-bpm"]');
                        if (bpmScrollList) {
                            var bpmText = bpmScrollList.querySelector('.scroll-list-text');
                            if (bpmText) {
                                if (value) {
                                    // Sync is ON - show "DAW"
                                    bpmText.setAttribute('data-original-text', bpmText.textContent);
                                    bpmText.textContent = 'DAW';
                                    bpmScrollList.style.pointerEvents = 'none';
                                } else {
                                    // Sync is OFF - restore BPM value
                                    var originalText = bpmText.getAttribute('data-original-text');
                                    if (originalText) {
                                        bpmText.textContent = originalText;
                                    }
                                    bpmScrollList.style.pointerEvents = 'auto';
                                }
                            }
                        }
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = control.value;
            }
        })();
    }

    // Initialize all momentary buttons
    var momentaryButtons = document.querySelectorAll('[data-momentary-id]');
    for (var i = 0; i < momentaryButtons.length; i++) {
        (function() {
            var element = momentaryButtons[i];
            var id = element.getAttribute('data-momentary-id');
            var valueElement = valueElements[id];

            var control = that.createMomentaryButton(element, {
                id: id,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = value;
                    }
                }
            });

            that.controls[id] = control;
        })();
    }

    // Initialize all XY pads
    var xyPads = document.querySelectorAll('[data-xy-id]');
    for (var i = 0; i < xyPads.length; i++) {
        (function() {
            var container = xyPads[i];
            var id = container.getAttribute('data-xy-id');
            var valueElement = valueElements[id];

            var control = that.createXYPad(container, {
                id: id,
                onChange: function(x, y) {
                    if (valueElement) {
                        valueElement.textContent = 'X: ' + x + ', Y: ' + y;
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = 'X: ' + control.x + ', Y: ' + control.y;
            }
        })();
    }

    // Initialize all dropdowns
    var dropdowns = document.querySelectorAll('[data-dropdown-id]');
    for (var i = 0; i < dropdowns.length; i++) {
        (function() {
            var container = dropdowns[i];
            var id = container.getAttribute('data-dropdown-id');
            var valueElement = valueElements[id];

            var control = that.createDropdown(container, {
                id: id,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = value;
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = control.value;
            }
        })();
    }

    // Initialize all scroll lists
    var scrollLists = document.querySelectorAll('[data-scroll-list-id]');
    for (var i = 0; i < scrollLists.length; i++) {
        (function() {
            var container = scrollLists[i];
            var id = container.getAttribute('data-scroll-list-id');
            var valueElement = valueElements[id];

            // Get options from data attribute or use defaults
            var optionsAttr = container.getAttribute('data-options');
            var options = optionsAttr ? optionsAttr.split(',') : ['Option 1', 'Option 2', 'Option 3', 'Option 4'];

            // Get initial value from data attribute
            var initialValueAttr = container.getAttribute('data-initial-value');
            var initialValue = initialValueAttr ? parseInt(initialValueAttr) : 0;

            var control = that.createScrollList(container, {
                id: id,
                options: options,
                value: initialValue,
                onChange: function(value) {
                    if (valueElement) {
                        valueElement.textContent = value;
                    }
                }
            });

            that.controls[id] = control;

            if (valueElement) {
                valueElement.textContent = control.value;
            }
        })();
    }

    // Initialize VB (Velocity Bender)
    var vbCanvas = document.querySelector('[data-vb-canvas="1"]');
    if (vbCanvas) {
        var vbController = new VelocityBenderController();
        var vbDisplay = new VBWaveformDisplay(vbCanvas, vbController);

        // Get the random/reset button and label references
        var randBtn = document.querySelector('[data-vb-random="1"]');
        var vbButtonLabel = document.getElementById('vb-button-label');

        // Function to update the random/reset button label
        var updateVbButtonLabel = function() {
            if (vbButtonLabel && vbController) {
                if (vbController.isFlat()) {
                    vbButtonLabel.textContent = 'RANDOM';
                } else {
                    vbButtonLabel.textContent = 'RESET';
                }
            }
        };

        // Connect bipolar knobs to VB controller
        var vbKnobs = ['instrument-vel-2n', 'instrument-vel-4n', 'instrument-vel-4nt', 'instrument-vel-8n'];
        for (var i = 0; i < vbKnobs.length; i++) {
            (function(index) {
                var knobControl = that.controls[vbKnobs[index]];
                if (knobControl && knobControl.onChange) {
                    var originalOnChange = knobControl.onChange;
                    knobControl.onChange = function(value) {
                        if (originalOnChange) originalOnChange(value);
                        vbController.setSliderValue(index, value);
                        vbDisplay.draw();
                        updateVbButtonLabel();
                    };
                }
            })(i);
        }

        // Connect Rand button
        if (randBtn) {
            randBtn.addEventListener('pointerdown', function(e) {
                e.preventDefault();
                // Add flash animation
                randBtn.classList.add('button-flash');
                setTimeout(function() {
                    randBtn.classList.remove('button-flash');
                }, 200);

                // Check if LFO is flat
                if (vbController.isFlat()) {
                    // LFO is flat - randomize
                    vbController.randomize();
                    // Update knob displays with random values
                    for (var i = 0; i < 4; i++) {
                        var knob = that.controls[vbKnobs[i]];
                        if (knob) {
                            knob.value = vbController.sliderValues[i];
                            knob.updateDisplay();
                        }
                    }
                } else {
                    // LFO is not flat - reset
                    vbController.reset();
                    // Update knob displays to 0
                    for (var i = 0; i < 4; i++) {
                        var knob = that.controls[vbKnobs[i]];
                        if (knob) {
                            knob.value = 0;
                            knob.updateDisplay();
                        }
                    }
                }
                vbDisplay.draw();
                updateVbButtonLabel();
            });
        }

        // Draw initial waveform
        vbDisplay.draw();

        // Set initial button label
        updateVbButtonLabel();

        that.controls['vb'] = { controller: vbController, display: vbDisplay };
    }

    // Initialize Euclidean STEPS/PULSES/START ON constraints
    that.initializeEuclideanConstraints();

    // Initialize Mix Matrix
    var mmToggle = document.querySelector('.mm-m1-toggle');
    if (mmToggle) {
        var mixMatrix = new MixMatrixController();
        mixMatrix.initializeControls();
        that.controls['mixMatrix'] = mixMatrix;
    }

    // Set initial disabled states for Fixed toggle sliders (after all knobs are created)
    for (var controlId in that.controls) {
        if (that.controls.hasOwnProperty(controlId)) {
            var control = that.controls[controlId];
            if (control._needsInitialState) {
                // Trigger onChange to set initial disabled states
                // Fixed starts as OFF (left/false), so Scale should be enabled, Level should be disabled
                if (control.onChange) {
                    control.onChange(control.value || false);
                }
            }
        }
    }
};

/* ============================================
   EUCLIDEAN STEPS/PULSES/START ON CONSTRAINTS
   ============================================ */

MotorikGUI.prototype.initializeEuclideanConstraints = function() {
    var that = this;

    // Define all rows with euclidean controls
    var rows = ['bd', 'bdacc', 'sn', 'snacc', 'hh', 'hhacc'];

    // Helper function to update options for a scroll list
    function updateScrollListOptions(control, newOptions, currentDisplayValue) {
        if (!control) return;

        // Find the index of the current display value in the new options
        var currentValue = control.options[control.value];
        var newIndex = newOptions.indexOf(currentValue);

        // If current value doesn't exist in new options, find the closest valid value
        if (newIndex === -1) {
            // Try to parse as number and find closest
            var currentNum = parseInt(currentValue);
            if (!isNaN(currentNum)) {
                // Find the maximum valid value in new options
                var maxValue = Math.max.apply(Math, newOptions.map(function(opt) {
                    return parseInt(opt);
                }));
                newIndex = newOptions.indexOf(String(Math.min(currentNum, maxValue)));

                // If still not found, use last option
                if (newIndex === -1) {
                    newIndex = newOptions.length - 1;
                }
            } else {
                // Use last option as fallback
                newIndex = newOptions.length - 1;
            }
        }

        // Update the control
        control.options = newOptions;
        control.value = newIndex;
        control.updateDisplay();

        if (control.onChange) {
            control.onChange(control.value);
        }
    }

    // For each row, set up the STEPS -> PULSES/START ON dependency
    rows.forEach(function(row) {
        var stepsControl = that.controls[row + '-steps'];
        var pulsesControl = that.controls[row + '-pulses'];
        var startOnControl = that.controls[row + '-start-on'];

        if (!stepsControl || !pulsesControl || !startOnControl) return;

        // Store original onChange handlers
        var originalStepsOnChange = stepsControl.onChange;

        // Override STEPS onChange to update PULSES and START ON constraints
        stepsControl.onChange = function(valueIndex) {
            // Call original handler
            if (originalStepsOnChange) {
                originalStepsOnChange(valueIndex);
            }

            // Get current STEPS value (it's a string like "16")
            var stepsValue = parseInt(stepsControl.options[valueIndex]);

            // Update PULSES options (0 to STEPS)
            var pulsesOptions = [];
            for (var i = 0; i <= stepsValue; i++) {
                pulsesOptions.push(String(i));
            }
            updateScrollListOptions(pulsesControl, pulsesOptions);

            // Update START ON options (1 to STEPS)
            var startOnOptions = [];
            for (var i = 1; i <= stepsValue; i++) {
                startOnOptions.push(String(i));
            }
            updateScrollListOptions(startOnControl, startOnOptions);
        };

        // Trigger initial update to set correct constraints
        if (stepsControl.onChange) {
            stepsControl.onChange(stepsControl.value);
        }
    });
};

/* ============================================
   CLEANUP AND MEMORY MANAGEMENT
   ============================================ */

/**
 * Destroys the MotorikGUI instance and cleans up all event listeners
 * Call this method before removing the UI to prevent memory leaks
 */
MotorikGUI.prototype.destroy = function() {
    // Destroy all controls
    for (var id in this.controls) {
        if (this.controls.hasOwnProperty(id)) {
            var control = this.controls[id];

            // If control has a destroy method, call it
            if (control && typeof control.destroy === 'function') {
                control.destroy();
            }
        }
    }

    // Clear controls object
    this.controls = {};

    // Remove window resize listener
    // Note: In a production implementation, we'd need to store the bound handler
    // For now, this serves as documentation for future improvements
};

/**
 * Adds destroy method to a control object
 * @param {Object} control - Control object to enhance
 * @param {HTMLElement} element - Main element of the control
 * @param {Array} eventHandlers - Array of {element, event, handler} objects
 */
MotorikGUI.prototype.addControlDestroyMethod = function(control, element, eventHandlers) {
    control.destroy = function() {
        // Remove all event listeners
        if (eventHandlers && eventHandlers.length > 0) {
            for (var i = 0; i < eventHandlers.length; i++) {
                var handler = eventHandlers[i];
                handler.element.removeEventListener(handler.event, handler.handler);
            }
        }

        // Remove DOM element if it was created by the control
        if (element && element.parentNode) {
            element.parentNode.removeChild(element);
        }

        // Clear references
        control.element = null;
        control.onChange = null;
    };
};

/* ============================================
   PUBLIC API - GET/SET CONTROL VALUES
   ============================================ */

MotorikGUI.prototype.getValue = function(id) {
    var control = this.controls[id];
    if (!control) return null;

    if (control.x !== undefined && control.y !== undefined) {
        return { x: control.x, y: control.y };
    }

    return control.value;
};

MotorikGUI.prototype.setValue = function(id, value) {
    var control = this.controls[id];
    if (!control) return;

    if (typeof value === 'object' && value.x !== undefined && value.y !== undefined) {
        control.x = value.x;
        control.y = value.y;
    } else {
        control.value = value;
    }

    if (control.updateDisplay) {
        control.updateDisplay();
    }
};

MotorikGUI.prototype.getControl = function(id) {
    return this.controls[id];
};

/* ============================================
   VELOCITY BENDER (VB) CONTROLLER
   ============================================ */

// Velocity Bender Controller
function VelocityBenderController() {
    // Slider values (-1.0 to 1.0)
    this.sliderValues = [0.0, 0.0, 0.0, 0.0];

    // Beat divisions (matching C++ implementation)
    this.beatDivisions = [0.5, 0.25, 1.0/6.0, 0.125];

    // Per-instrument enable states
    this.instrumentEnabled = [true, true, true, true, true, true];
}

VelocityBenderController.prototype.setSliderValue = function(index, value) {
    if (index >= 0 && index < 4) {
        // Clamp to -1.0 to 1.0
        this.sliderValues[index] = Math.max(-1.0, Math.min(1.0, value));
    }
};

VelocityBenderController.prototype.setInstrumentEnabled = function(index, enabled) {
    if (index >= 0 && index < 6) {
        this.instrumentEnabled[index] = enabled;
    }
};

VelocityBenderController.prototype.randomize = function() {
    for (var i = 0; i < 4; i++) {
        this.sliderValues[i] = Math.random() * 2.0 - 1.0;
    }
};

VelocityBenderController.prototype.reset = function() {
    this.sliderValues = [0.0, 0.0, 0.0, 0.0];
};

VelocityBenderController.prototype.isFlat = function() {
    // Check if all slider values are at 0 (flat line)
    for (var i = 0; i < 4; i++) {
        if (this.sliderValues[i] !== 0.0) {
            return false;
        }
    }
    return true;
};

// Calculate LFO value at given phase (0.0 - 1.0)
VelocityBenderController.prototype.calculateLFOValue = function(phase) {
    var value = 0.0;
    var totalWeight = 0.0;

    for (var i = 0; i < 4; i++) {
        // Calculate frequency based on beat division
        var frequency = 1.0 / this.beatDivisions[i];

        // Phase shift (matching C++ implementation)
        var phaseShift;
        if (i === 0) {  // 1/2 beat (2n)
            phaseShift = -0.125;  // Shift by -1/8
        } else if (i === 1) {  // 1/4 beat (4n)
            phaseShift = -0.0625;  // Shift by -1/16
        } else if (i === 2) {  // 1/6 beat (4nt)
            phaseShift = -0.0417;  // Shift by -1/24
        } else {  // 1/8 beat (8n)
            phaseShift = -0.03125;  // Shift by -1/32
        }

        // Create a sine wave at this frequency with phase shift
        var adjustedPhase = phase + phaseShift;
        var sineValue = Math.sin(2.0 * Math.PI * frequency * adjustedPhase);

        // Invert the slider value so up = positive curve starts high
        var invertedSliderValue = -this.sliderValues[i];

        // Weight by slider value
        var weight = Math.abs(invertedSliderValue);
        value += sineValue * invertedSliderValue;
        totalWeight += weight;
    }

    // Normalize if we have any active sliders
    if (totalWeight > 0.001) {
        value /= (totalWeight + 1.0); // Soft normalization
    }

    // Apply smoothing function (tanh for soft saturation)
    value = Math.tanh(value * 1.5);

    return value;
};

// Get waveform for display
VelocityBenderController.prototype.getWaveformForDisplay = function(numPoints) {
    numPoints = numPoints || 550;
    var waveform = [];

    for (var i = 0; i < numPoints; i++) {
        var phase = i / (numPoints - 1);
        var lfoValue = this.calculateLFOValue(phase);

        // Normalize to 0-1 range for display
        waveform.push((lfoValue + 1.0) * 0.5);
    }

    return waveform;
};

// Waveform Display
function VBWaveformDisplay(canvas, controller) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.controller = controller;
}

VBWaveformDisplay.prototype.draw = function() {
    var width = this.canvas.width;
    var height = this.canvas.height;

    // Clear canvas
    this.ctx.fillStyle = '#101010';
    this.ctx.fillRect(0, 0, width, height);

    // Draw grid lines
    this.drawGrid(width, height);

    // Draw waveform
    this.drawWaveform(width, height);

    // Draw border
    this.ctx.strokeStyle = '#595e5f';
    this.ctx.lineWidth = 1;
    this.ctx.strokeRect(0, 0, width, height);
};

VBWaveformDisplay.prototype.drawGrid = function(width, height) {
    // Main vertical grid lines at beat divisions (4 divisions for one bar)
    this.ctx.strokeStyle = '#595e5f';
    this.ctx.lineWidth = 0.5;
    for (var i = 1; i < 4; i++) {
        var x = (width / 4) * i;
        this.ctx.beginPath();
        this.ctx.moveTo(x, 0);
        this.ctx.lineTo(x, height);
        this.ctx.stroke();
    }

    // Additional subdivision lines (8th notes)
    this.ctx.strokeStyle = '#4a4e4f';
    this.ctx.lineWidth = 0.4;
    for (var i = 1; i < 8; i++) {
        if (i % 2 !== 0) {  // Skip main beat lines
            var x = (width / 8) * i;
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, height);
            this.ctx.stroke();
        }
    }

    // Even finer subdivision lines (16th notes)
    this.ctx.strokeStyle = '#3a3e3f';
    this.ctx.lineWidth = 0.3;
    for (var i = 1; i < 16; i++) {
        if (i % 2 !== 0) {  // Skip 8th note lines
            var x = (width / 16) * i;
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, height);
            this.ctx.stroke();
        }
    }

    // Horizontal center line
    this.ctx.strokeStyle = '#595e5f';
    this.ctx.lineWidth = 0.5;
    this.ctx.beginPath();
    this.ctx.moveTo(0, height * 0.5);
    this.ctx.lineTo(width, height * 0.5);
    this.ctx.stroke();
};

VBWaveformDisplay.prototype.drawWaveform = function(width, height) {
    var waveformData = this.controller.getWaveformForDisplay(width);

    this.ctx.strokeStyle = COLORS.accent;
    this.ctx.lineWidth = 2;
    this.ctx.beginPath();

    for (var i = 0; i < waveformData.length; i++) {
        var x = i;
        var y = height * (1.0 - waveformData[i]);

        if (i === 0) {
            this.ctx.moveTo(x, y);
        } else {
            this.ctx.lineTo(x, y);
        }
    }

    this.ctx.stroke();
};

/* ============================================
   MIX MATRIX CONTROLLER
   ============================================ */

function MixMatrixController() {
    this.NUM_INSTRUMENTS = 6;
    this.NUM_PRIORITIES = 6;

    // State storage
    this.priorities = [0, 0, 0, 0, 0, 0]; // Each instrument's priority (0-5)
    this.m1States = [false, false, false, false, false, false];
    this.m2States = [false, false, false, false, false, false];
    this.s1States = [false, false, false, false, false, false];
    this.s2States = [false, false, false, false, false, false];
    this.p1Values = [100, 100, 100, 100, 100, 100];
    this.p2Values = [100, 100, 100, 100, 100, 100];
}

MixMatrixController.prototype.initializeControls = function() {
    var that = this;

    // Priority Radio Buttons
    var priorityRadios = document.querySelectorAll('.mm-priority-radio');
    priorityRadios.forEach(function(radio) {
        radio.addEventListener('change', function(e) {
            if (e.target.checked) {
                var row = parseInt(e.target.getAttribute('data-mm-row'));
                var priority = parseInt(e.target.getAttribute('data-mm-priority'));
                that.onPriorityChanged(row, priority);
            }
        });

        // Double-click to set all buttons in the same column
        radio.addEventListener('dblclick', function(e) {
            var priority = parseInt(e.target.getAttribute('data-mm-priority'));

            // Set all radio buttons in this priority column across all rows
            for (var i = 0; i < 6; i++) {
                var radioInRow = document.querySelector('.mm-priority-radio[data-mm-row="' + i + '"][data-mm-priority="' + priority + '"]');
                if (radioInRow) {
                    radioInRow.checked = true;
                    that.onPriorityChanged(i, priority);
                }
            }
        });
    });

    // M1 Toggle Buttons
    var m1Toggles = document.querySelectorAll('.mm-m1-toggle');
    m1Toggles.forEach(function(toggle) {
        toggle.addEventListener('pointerdown', function(e) {
            e.preventDefault();
            var row = e.target.getAttribute('data-mm-m1-toggle');
            var isActive = e.target.classList.contains('active');
            var newState = !isActive;

            if (newState) {
                e.target.classList.add('active');
            } else {
                e.target.classList.remove('active');
            }

            that.onM1Changed(row, newState);
        });
    });

    // M2 Toggle Buttons
    var m2Toggles = document.querySelectorAll('.mm-m2-toggle');
    m2Toggles.forEach(function(toggle) {
        toggle.addEventListener('pointerdown', function(e) {
            e.preventDefault();
            var row = e.target.getAttribute('data-mm-m2-toggle');
            var isActive = e.target.classList.contains('active');
            var newState = !isActive;

            if (newState) {
                e.target.classList.add('active');
            } else {
                e.target.classList.remove('active');
            }

            that.onM2Changed(row, newState);
        });
    });

    // S1 Toggle Buttons
    var s1Toggles = document.querySelectorAll('.mm-s1-toggle');
    s1Toggles.forEach(function(toggle) {
        toggle.addEventListener('pointerdown', function(e) {
            e.preventDefault();
            var row = e.target.getAttribute('data-mm-s1-toggle');
            var isActive = e.target.classList.contains('active');
            var newState = !isActive;

            if (newState) {
                e.target.classList.add('active');
            } else {
                e.target.classList.remove('active');
            }

            that.onS1Changed(row, newState);
        });
    });

    // S2 Toggle Buttons
    var s2Toggles = document.querySelectorAll('.mm-s2-toggle');
    s2Toggles.forEach(function(toggle) {
        toggle.addEventListener('pointerdown', function(e) {
            e.preventDefault();
            var row = e.target.getAttribute('data-mm-s2-toggle');
            var isActive = e.target.classList.contains('active');
            var newState = !isActive;

            if (newState) {
                e.target.classList.add('active');
            } else {
                e.target.classList.remove('active');
            }

            that.onS2Changed(row, newState);
        });
    });

    // P1 Sliders
    var p1Sliders = document.querySelectorAll('.mm-p1-slider');
    p1Sliders.forEach(function(slider) {
        slider.addEventListener('input', function(e) {
            var row = parseInt(e.target.getAttribute('data-mm-row'));
            that.p1Values[row] = parseInt(e.target.value);
        });
    });

    // P2 Sliders
    var p2Sliders = document.querySelectorAll('.mm-p2-slider');
    p2Sliders.forEach(function(slider) {
        slider.addEventListener('input', function(e) {
            var row = parseInt(e.target.getAttribute('data-mm-row'));
            that.p2Values[row] = parseInt(e.target.value);
        });
    });

    // All Row Checkboxes
    var allCheckboxes = document.querySelectorAll('.mm-all-checkbox');
    allCheckboxes.forEach(function(checkbox) {
        checkbox.addEventListener('change', function(e) {
            var column = e.target.getAttribute('data-mm-column');
            if (column === 'priority') {
                var priority = parseInt(e.target.getAttribute('data-mm-priority'));
                that.onAllPriorityClicked(priority);
            } else {
                that.onAllColumnClicked(column);
            }
        });
    });

    // 100% Buttons
    var p1All100Btn = document.getElementById('mm-p1-all-100');
    if (p1All100Btn) {
        p1All100Btn.addEventListener('click', function() {
            that.setAllP1To100();
        });
    }

    var p2All100Btn = document.getElementById('mm-p2-all-100');
    if (p2All100Btn) {
        p2All100Btn.addEventListener('click', function() {
            that.setAllP2To100();
        });
    }

    this.updateAllRowCheckboxes();
};

// Priority changed for a single instrument
MixMatrixController.prototype.onPriorityChanged = function(row, priority) {
    this.priorities[row] = priority;
    this.updateAllRowCheckboxes();
};

// M1 changed - mutually exclusive with S1
MixMatrixController.prototype.onM1Changed = function(row, checked) {
    this.m1States[row] = checked;

    // If M1 is being enabled, disable S1
    if (checked && this.s1States[row]) {
        this.s1States[row] = false;
        var s1Toggle = document.querySelector('.mm-s1-toggle[data-mm-s1-toggle="' + row + '"]');
        if (s1Toggle) {
            s1Toggle.classList.remove('active');
        }
    }

    this.updateAllRowCheckboxes();
};

// M2 changed - mutually exclusive with S2
MixMatrixController.prototype.onM2Changed = function(row, checked) {
    this.m2States[row] = checked;

    // If M2 is being enabled, disable S2
    if (checked && this.s2States[row]) {
        this.s2States[row] = false;
        var s2Toggle = document.querySelector('.mm-s2-toggle[data-mm-s2-toggle="' + row + '"]');
        if (s2Toggle) {
            s2Toggle.classList.remove('active');
        }
    }

    this.updateAllRowCheckboxes();
};

// S1 changed - mutually exclusive with M1
MixMatrixController.prototype.onS1Changed = function(row, checked) {
    this.s1States[row] = checked;

    // If S1 is being enabled, disable M1
    if (checked && this.m1States[row]) {
        this.m1States[row] = false;
        var m1Toggle = document.querySelector('.mm-m1-toggle[data-mm-m1-toggle="' + row + '"]');
        if (m1Toggle) {
            m1Toggle.classList.remove('active');
        }
    }

    this.updateAllRowCheckboxes();
};

// S2 changed - mutually exclusive with M2
MixMatrixController.prototype.onS2Changed = function(row, checked) {
    this.s2States[row] = checked;

    // If S2 is being enabled, disable M2
    if (checked && this.m2States[row]) {
        this.m2States[row] = false;
        var m2Toggle = document.querySelector('.mm-m2-toggle[data-mm-m2-toggle="' + row + '"]');
        if (m2Toggle) {
            m2Toggle.classList.remove('active');
        }
    }

    this.updateAllRowCheckboxes();
};

// All row clicked for a column (M1, S1, S2, M2)
MixMatrixController.prototype.onAllColumnClicked = function(column) {
    var states, checkboxClass;

    switch (column) {
        case 'm1':
            states = this.m1States;
            checkboxClass = '.mm-m1-checkbox';
            break;
        case 'm2':
            states = this.m2States;
            checkboxClass = '.mm-m2-checkbox';
            break;
        case 's1':
            states = this.s1States;
            checkboxClass = '.mm-s1-checkbox';
            break;
        case 's2':
            states = this.s2States;
            checkboxClass = '.mm-s2-checkbox';
            break;
        default:
            return;
    }

    // Count how many are currently enabled
    var enabledCount = 0;
    for (var i = 0; i < states.length; i++) {
        if (states[i]) enabledCount++;
    }

    // TOGGLE behavior: if all are enabled, disable all; otherwise enable all
    var shouldEnable = enabledCount < this.NUM_INSTRUMENTS;

    for (var row = 0; row < this.NUM_INSTRUMENTS; row++) {
        states[row] = shouldEnable;
        var checkbox = document.querySelector(checkboxClass + '[data-mm-row="' + row + '"]');
        if (checkbox) checkbox.checked = shouldEnable;

        // Handle mutual exclusion
        if (shouldEnable) {
            if (column === 'm1' && this.m2States[row]) {
                this.m2States[row] = false;
                var m2Cb = document.querySelector('.mm-m2-checkbox[data-mm-row="' + row + '"]');
                if (m2Cb) m2Cb.checked = false;
            } else if (column === 'm2' && this.m1States[row]) {
                this.m1States[row] = false;
                var m1Cb = document.querySelector('.mm-m1-checkbox[data-mm-row="' + row + '"]');
                if (m1Cb) m1Cb.checked = false;
            } else if (column === 's1' && this.s2States[row]) {
                this.s2States[row] = false;
                var s2Cb = document.querySelector('.mm-s2-checkbox[data-mm-row="' + row + '"]');
                if (s2Cb) s2Cb.checked = false;
            } else if (column === 's2' && this.s1States[row]) {
                this.s1States[row] = false;
                var s1Cb = document.querySelector('.mm-s1-checkbox[data-mm-row="' + row + '"]');
                if (s1Cb) s1Cb.checked = false;
            }
        }
    }

    this.updateAllRowCheckboxes();
};

// All row clicked for a priority column
MixMatrixController.prototype.onAllPriorityClicked = function(priority) {
    // Count how many instruments are at this priority
    var enabledCount = 0;
    for (var i = 0; i < this.priorities.length; i++) {
        if (this.priorities[i] === priority) enabledCount++;
    }

    // NO-OP behavior: if all are already at this priority, do nothing
    if (enabledCount === this.NUM_INSTRUMENTS) {
        // Keep checkbox checked (do nothing)
        var allCheckbox = document.getElementById('mm-all-priority-' + priority);
        if (allCheckbox) allCheckbox.checked = true;
        return;
    }

    // Otherwise, set all instruments to this priority
    for (var row = 0; row < this.NUM_INSTRUMENTS; row++) {
        this.priorities[row] = priority;
        var radio = document.querySelector('.mm-priority-radio[data-mm-row="' + row + '"][data-mm-priority="' + priority + '"]');
        if (radio) radio.checked = true;
    }

    this.updateAllRowCheckboxes();
};

// Update "All" row checkboxes to reflect current state
MixMatrixController.prototype.updateAllRowCheckboxes = function() {
    // M1 column
    var m1EnabledCount = 0;
    for (var i = 0; i < this.m1States.length; i++) {
        if (this.m1States[i]) m1EnabledCount++;
    }
    var allM1 = document.getElementById('mm-all-m1');
    if (allM1) allM1.checked = (m1EnabledCount === this.NUM_INSTRUMENTS);

    // M2 column
    var m2EnabledCount = 0;
    for (var i = 0; i < this.m2States.length; i++) {
        if (this.m2States[i]) m2EnabledCount++;
    }
    var allM2 = document.getElementById('mm-all-m2');
    if (allM2) allM2.checked = (m2EnabledCount === this.NUM_INSTRUMENTS);

    // S1 column
    var s1EnabledCount = 0;
    for (var i = 0; i < this.s1States.length; i++) {
        if (this.s1States[i]) s1EnabledCount++;
    }
    var allS1 = document.getElementById('mm-all-s1');
    if (allS1) allS1.checked = (s1EnabledCount === this.NUM_INSTRUMENTS);

    // S2 column
    var s2EnabledCount = 0;
    for (var i = 0; i < this.s2States.length; i++) {
        if (this.s2States[i]) s2EnabledCount++;
    }
    var allS2 = document.getElementById('mm-all-s2');
    if (allS2) allS2.checked = (s2EnabledCount === this.NUM_INSTRUMENTS);

    // Priority columns (1-6)
    for (var priority = 0; priority < this.NUM_PRIORITIES; priority++) {
        var enabledCount = 0;
        for (var i = 0; i < this.priorities.length; i++) {
            if (this.priorities[i] === priority) enabledCount++;
        }
        var allCheckbox = document.getElementById('mm-all-priority-' + priority);
        if (allCheckbox) allCheckbox.checked = (enabledCount === this.NUM_INSTRUMENTS);
    }
};

// Set all P1 sliders to 100%
MixMatrixController.prototype.setAllP1To100 = function() {
    var p1Sliders = document.querySelectorAll('.mm-p1-slider');
    for (var i = 0; i < p1Sliders.length; i++) {
        p1Sliders[i].value = 100;
        this.p1Values[i] = 100;
    }
};

// Set all P2 sliders to 100%
MixMatrixController.prototype.setAllP2To100 = function() {
    var p2Sliders = document.querySelectorAll('.mm-p2-slider');
    for (var i = 0; i < p2Sliders.length; i++) {
        p2Sliders[i].value = 100;
        this.p2Values[i] = 100;
    }
};

// Get current state (for debugging or export)
MixMatrixController.prototype.getState = function() {
    return {
        priorities: this.priorities.slice(),
        m1: this.m1States.slice(),
        m2: this.m2States.slice(),
        s1: this.s1States.slice(),
        s2: this.s2States.slice(),
        p1: this.p1Values.slice(),
        p2: this.p2Values.slice()
    };
};

/* ============================================
   PRESET NAVIGATION
   ============================================ */

// Initialize preset navigation buttons
function initializePresetNavigation() {
    var presetUp = document.querySelector('.nav-btn-up');
    var presetDown = document.querySelector('.nav-btn-down');
    var presetSelect = document.querySelector('.preset-select');

    if (!presetUp || !presetDown || !presetSelect) {
        return; // Elements not found
    }

    // Store the current preset index (initially the first actual preset after the actions)
    var currentPresetIndex = 6; // Index of "Dense (12:5)" - first actual preset
    presetSelect.selectedIndex = currentPresetIndex;

    // Handle dropdown selection
    presetSelect.addEventListener('change', function() {
        var selectedOption = presetSelect.options[presetSelect.selectedIndex];
        var value = selectedOption.value;

        // Check if an action was selected
        if (value === 'save') {
            console.log('Save preset clicked');
            alert('Save preset functionality to be implemented');
            // Reset to current preset
            presetSelect.selectedIndex = currentPresetIndex;
        } else if (value === 'save-as') {
            console.log('Save As preset clicked');
            var presetName = prompt('Enter preset name:');
            if (presetName) {
                console.log('Saving preset as:', presetName);
                alert('Save As functionality to be implemented for: ' + presetName);
            }
            // Reset to current preset
            presetSelect.selectedIndex = currentPresetIndex;
        } else if (value === 'delete') {
            console.log('Delete preset clicked');
            if (confirm('Are you sure you want to delete this preset?')) {
                console.log('Deleting preset');
                alert('Delete functionality to be implemented');
            }
            // Reset to current preset
            presetSelect.selectedIndex = currentPresetIndex;
        } else if (value === 'random') {
            console.log('Random preset clicked');
            alert('Random preset generation to be implemented');
            // Reset to current preset
            presetSelect.selectedIndex = currentPresetIndex;
        } else if (!selectedOption.disabled) {
            // A real preset was selected, update the current preset index
            currentPresetIndex = presetSelect.selectedIndex;
            console.log('Preset selected:', selectedOption.text);
        }
    });

    presetUp.addEventListener('click', function() {
        var currentIndex = presetSelect.selectedIndex;
        // Navigate to previous preset (skip actions and separators)
        var newIndex = currentIndex - 1;
        while (newIndex >= 6) {
            if (!presetSelect.options[newIndex].disabled && !isActionOption(presetSelect.options[newIndex])) {
                presetSelect.selectedIndex = newIndex;
                currentPresetIndex = newIndex;
                return;
            }
            newIndex--;
        }
    });

    presetDown.addEventListener('click', function() {
        var currentIndex = presetSelect.selectedIndex;
        // Navigate to next preset (skip actions and separators)
        var newIndex = currentIndex + 1;
        while (newIndex < presetSelect.options.length) {
            if (!presetSelect.options[newIndex].disabled && !isActionOption(presetSelect.options[newIndex])) {
                presetSelect.selectedIndex = newIndex;
                currentPresetIndex = newIndex;
                return;
            }
            newIndex++;
        }
    });

    // Helper function to check if an option is an action
    function isActionOption(option) {
        var value = option.value;
        return value === 'save' || value === 'save-as' || value === 'delete' || value === 'random';
    }
}

/* ============================================
   MIDI OUTPUT NAVIGATION
   ============================================ */

// Initialize MIDI output select with navigation
function initializeMIDIOutput() {
    var outputUp = document.querySelector('.midi-output-up');
    var outputDown = document.querySelector('.midi-output-down');
    var outputSelect = document.querySelector('.midi-output-select');

    if (!outputUp || !outputDown || !outputSelect) {
        return; // Elements not found
    }

    // Store the current output index
    var currentOutputIndex = 0;
    outputSelect.selectedIndex = currentOutputIndex;

    // Handle dropdown selection
    outputSelect.addEventListener('change', function() {
        currentOutputIndex = outputSelect.selectedIndex;
        console.log('Output selected:', outputSelect.options[currentOutputIndex].text);
    });

    outputUp.addEventListener('click', function() {
        var currentIndex = outputSelect.selectedIndex;
        if (currentIndex > 0) {
            outputSelect.selectedIndex = currentIndex - 1;
            currentOutputIndex = outputSelect.selectedIndex;
        }
    });

    outputDown.addEventListener('click', function() {
        var currentIndex = outputSelect.selectedIndex;
        if (currentIndex < outputSelect.options.length - 1) {
            outputSelect.selectedIndex = currentIndex + 1;
            currentOutputIndex = outputSelect.selectedIndex;
        }
    });
}

/* ============================================
   MIDI MAPPING NAVIGATION
   ============================================ */

// Initialize MIDI mapping select with navigation
function initializeMIDIMapping() {
    var mappingUp = document.querySelector('.midi-mapping-up');
    var mappingDown = document.querySelector('.midi-mapping-down');
    var mappingSelect = document.querySelector('.midi-mapping-select');

    if (!mappingUp || !mappingDown || !mappingSelect) {
        return; // Elements not found
    }

    // Store the current mapping index (initially the first actual mapping after the actions)
    var currentMappingIndex = 4; // Index of "GM Standard" - first actual mapping
    mappingSelect.selectedIndex = currentMappingIndex;

    // Handle dropdown selection
    mappingSelect.addEventListener('change', function() {
        var selectedOption = mappingSelect.options[mappingSelect.selectedIndex];
        var value = selectedOption.value;

        // Check if an action was selected
        if (value === 'save') {
            console.log('Save mapping clicked');
            alert('Save mapping functionality to be implemented');
            // Reset to current mapping
            mappingSelect.selectedIndex = currentMappingIndex;
        } else if (value === 'save-as') {
            console.log('Save As mapping clicked');
            var mappingName = prompt('Enter mapping name:');
            if (mappingName) {
                console.log('Saving mapping as:', mappingName);
                alert('Save As functionality to be implemented for: ' + mappingName);
            }
            // Reset to current mapping
            mappingSelect.selectedIndex = currentMappingIndex;
        } else if (value === 'delete') {
            console.log('Delete mapping clicked');
            if (confirm('Are you sure you want to delete this mapping?')) {
                console.log('Deleting mapping');
                alert('Delete functionality to be implemented');
            }
            // Reset to current mapping
            mappingSelect.selectedIndex = currentMappingIndex;
        } else if (!selectedOption.disabled) {
            // A real mapping was selected, update the current mapping index
            currentMappingIndex = mappingSelect.selectedIndex;
            console.log('Mapping selected:', selectedOption.text);
        }
    });

    mappingUp.addEventListener('click', function() {
        var currentIndex = mappingSelect.selectedIndex;
        // Navigate to previous mapping (skip actions and separators)
        var newIndex = currentIndex - 1;
        while (newIndex >= 4) {
            if (!mappingSelect.options[newIndex].disabled && !isActionOption(mappingSelect.options[newIndex])) {
                mappingSelect.selectedIndex = newIndex;
                currentMappingIndex = newIndex;
                return;
            }
            newIndex--;
        }
    });

    mappingDown.addEventListener('click', function() {
        var currentIndex = mappingSelect.selectedIndex;
        // Navigate to next mapping (skip actions and separators)
        var newIndex = currentIndex + 1;
        while (newIndex < mappingSelect.options.length) {
            if (!mappingSelect.options[newIndex].disabled && !isActionOption(mappingSelect.options[newIndex])) {
                mappingSelect.selectedIndex = newIndex;
                currentMappingIndex = newIndex;
                return;
            }
            newIndex++;
        }
    });

    // Helper function to check if an option is an action
    function isActionOption(option) {
        var value = option.value;
        return value === 'save' || value === 'save-as' || value === 'delete';
    }
}

/* ============================================
   TAB SWITCHING
   ============================================ */

// LED label word list
var ledWords = ['Boom', 'Thump', 'Doom', 'Buh', 'Puh', 'Umph', 'Tap', 'Tss', 'Tchak', 'Tuh', 'Tss', 'Tch', 'Sss', 'Chik', 'Ting', 'Tsik', 'Kss', 'Dum', 'Bap', 'Guh', 'Toh', 'Dug', 'Ksh', 'Klak', 'Tum'];

function randomizeLedLabel(tabId) {
    var randomWord = ledWords[Math.floor(Math.random() * ledWords.length)];
    var labelId = 'led-label-' + tabId.replace('tab', '');

    // Map tab IDs to label IDs
    var labelMap = {
        'tab1': 'led-label-pattern',
        'tab2': 'led-label-velocity',
        'tab3': 'led-label-mix',
        'tab4': 'led-label-midi'
    };

    var label = document.getElementById(labelMap[tabId]);
    if (label) {
        label.textContent = randomWord;
    }
}

function initializeTabs() {
    var tabHeaders = document.querySelectorAll('.tab-header');
    var tabContents = document.querySelectorAll('.tab-content');

    tabHeaders.forEach(function(header) {
        header.addEventListener('click', function() {
            var tabId = this.getAttribute('data-tab');

            // Remove active class from all headers
            tabHeaders.forEach(function(h) {
                h.classList.remove('active');
            });

            // Remove active class from all content
            tabContents.forEach(function(c) {
                c.classList.remove('active');
            });

            // Add active class to clicked header
            this.classList.add('active');

            // Show corresponding content
            var activeContent = document.getElementById(tabId);
            if (activeContent) {
                activeContent.classList.add('active');
            }

            // Randomize LED label for the new tab
            randomizeLedLabel(tabId);
        });
    });
}

/* ============================================
   LED INITIALIZATION
   ============================================ */

function initializeLeds() {
    var leds = document.querySelectorAll('.led');

    leds.forEach(function(led) {
        led.addEventListener('pointerdown', function(e) {
            e.preventDefault();
            // Add flash class
            this.classList.add('flash');

            // Remove flash class after 100ms for snappy effect
            var that = this;
            setTimeout(function() {
                that.classList.remove('flash');
            }, 100);
        });
    });
}

/* ============================================
   MAIN INITIALIZATION (disabled for landing page — init handled by inline script)
   ============================================ */
