/**
 * ============================================================================
 * BIRO CLASH: School Desk Physics
 * WebRTC PeerJS Zero-Cost Multiplayer + Pre-Game Menu Bridge
 * ============================================================================
 *
 * 100% Free Architecture:
 *  - Peer-to-peer WebRTC DataChannels (zero server data cost)
 *  - Public PeerJS cloud broker (0.peerjs.com) for SDP exchange
 *  - Google public STUN servers for NAT hole-punching
 *  - QR code generated client-side (no backend)
 */

(function () {
    'use strict';

    /* ------------------------------------------------------------------
       Network State
    ------------------------------------------------------------------ */
    const net = {
        peer:         null,
        conn:         null,
        roomCode:     '',
        fullPeerId:   '',
        isHost:       true,
        isConnected:  false,
        wasmReady:    false,
        pendingJoinCode: null,
        pendingSettings: null   // settings to apply once WASM is ready
    };

    const PEER_CONFIG = {
        debug: 1,
        config: {
            iceServers: [
                { urls: 'stun:stun.l.google.com:19302' },
                { urls: 'stun:stun1.l.google.com:19302' },
                { urls: 'stun:stun.services.mozilla.com' }
            ]
        }
    };

    /* ------------------------------------------------------------------
       Utilities
    ------------------------------------------------------------------ */
    function generateRoomCode() {
        // Exclude ambiguous characters: 0/O, 1/I, and confusing cursive letters like G and Z
        const chars = '23456789ABCDEFHJKLMNPQRSTUVWXY';
        let code = '';
        for (let i = 0; i < 5; i++) {
            code += chars.charAt(Math.floor(Math.random() * chars.length));
        }
        return code;
    }

    function getShareableRoomUrl() {
        const url = new URL(window.location.href);
        url.search = `?room=${net.roomCode}`;
        return url.toString();
    }

    function showToast(text, ms = 3500) {
        let toast = document.getElementById('toastNotice');
        if (!toast) {
            toast = document.createElement('div');
            toast.id = 'toastNotice';
            toast.className = 'toast-notice';
            document.body.appendChild(toast);
        }
        toast.textContent = text;
        toast.classList.add('show');
        clearTimeout(toast._t);
        toast._t = setTimeout(() => toast.classList.remove('show'), ms);
    }

    /* ------------------------------------------------------------------
       WASM C bridge helpers
    ------------------------------------------------------------------ */
    function c_setRoomCode(code) {
        if (!window.Module || !Module._SetOnlineRoomCode) return;
        try {
            const bytes = (code.length * 4) + 1;
            const ptr   = Module._malloc(bytes);
            Module.stringToUTF8(code, ptr, bytes);
            Module._SetOnlineRoomCode(ptr);
            Module._free(ptr);
        } catch (e) {
            console.warn('[MP] c_setRoomCode failed', e);
        }
    }

    function isActualMobileDevice() {
        const ua = navigator.userAgent || '';
        const isMobileOS = /Android|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(ua);
        const isDesktopOS = /Windows NT|Macintosh|X11|Linux x86_64/i.test(ua);
        if (isDesktopOS && !/Android/i.test(ua)) {
            return false; // Windows PC, Mac, or Linux Desktop!
        }
        return isMobileOS;
    }

    function c_applyMenuSettings(s) {
        if (!window.Module || !Module._SetMatchMode) return;
        try {
            if (Module._SetGameMode) Module._SetGameMode(s.mode || 0);
            if (Module._SetStageAxis) Module._SetStageAxis(s.stage || 0);
            if (Module._SetTableType) Module._SetTableType(s.table || 0);
            if (Module._SetMatchMode) Module._SetMatchMode(s.matchType || 0);
            if (Module._SetAIDifficulty) Module._SetAIDifficulty(s.aiDiff || 1);
            if (s.matchType === 2 && Module._SetOnlineRole) {
                Module._SetOnlineRole(net.isHost ? 1 : 0);
            }
            let touchMode = 0;
            if (s && typeof s.controlMode !== 'undefined') {
                touchMode = s.controlMode;
            } else {
                touchMode = isActualMobileDevice() ? 1 : 0;
            }
            if (Module._SetTouchControlMode) {
                Module._SetTouchControlMode(touchMode);
            }
            if (Module._SetGamePaused) {
                Module._SetGamePaused(0);
            }
        } catch (e) {
            console.warn('[MP] c_applyMenuSettings error', e);
        }
    }

    window.toggleNotebookMenu = function() {
        const menu = document.getElementById('mainMenu');
        if (!menu) return;
        if (menu.classList.contains('hidden')) {
            menu.classList.remove('hidden');
            if (window.Module && Module._SetGamePaused) {
                Module._SetGamePaused(1);
            }
            const startBtn = document.getElementById('startBtn');
            if (startBtn) startBtn.textContent = 'Resume Match  ▶';
        } else {
            menu.classList.add('hidden');
            if (window.Module && Module._SetGamePaused) {
                Module._SetGamePaused(0);
            }
        }
    };

    /* ------------------------------------------------------------------
       QR Code helpers (uses qrcode.js from CDN)
    ------------------------------------------------------------------ */
    function renderQRCode(url) {
        const box = document.getElementById('qrContainer');
        if (!box) return;
        box.innerHTML = '';
        try {
            new QRCode(box, {
                text:           url,
                width:          140,
                height:         140,
                colorDark:      '#111111',
                colorLight:     '#faf6ec',
                correctLevel:   QRCode.CorrectLevel.M
            });
        } catch (e) {
            box.innerHTML = '<span style="font-size:12px;color:#555">QR library not loaded</span>';
        }
    }

    /* ------------------------------------------------------------------
       Host Peer
    ------------------------------------------------------------------ */
    function initHost(code) {
        if (net.peer && !net.peer.destroyed && net.roomCode) return;

        net.isHost    = true;
        net.roomCode  = (code || generateRoomCode()).toUpperCase();
        net.fullPeerId = `biro-${net.roomCode}`;

        console.log(`[MP] Host initializing as ${net.fullPeerId}`);

        try {
            net.peer = new Peer(net.fullPeerId, PEER_CONFIG);
        } catch (e) {
            console.error('[MP] Peer init failed', e);
            return;
        }

        net.peer.on('open', () => {
            console.log(`[MP] Host peer open: ${net.fullPeerId}`);
            updateRoomCodeUI(net.roomCode);
            if (window.trackGameEvent) {
                window.trackGameEvent('multiplayer_host_created', { room_code: net.roomCode });
            }
            if (net.wasmReady) {
                c_setRoomCode(net.roomCode);
                if (Module._SetOnlineRole) Module._SetOnlineRole(1);
            }
        });

        net.peer.on('connection', (conn) => {
            console.log('[MP] Guest connected!');
            setupDataConnection(conn);
        });

        net.peer.on('error', (err) => {
            console.warn('[MP] Peer error:', err.type);
            if (err.type === 'unavailable-id') {
                net.peer.destroy();
                initHost(); // retry with fresh code
            }
        });
    }

    /* ------------------------------------------------------------------
       Guest Peer
    ------------------------------------------------------------------ */
    function initGuest(targetCode) {
        net.isHost    = false;
        net.roomCode  = targetCode.toUpperCase();
        net.fullPeerId = `biro-guest-${generateRoomCode()}`;

        console.log(`[MP] Guest joining room: ${net.roomCode}`);

        try {
            net.peer = new Peer(net.fullPeerId, PEER_CONFIG);
        } catch (e) {
            console.error('[MP] Guest peer init failed', e);
            return;
        }

        net.peer.on('open', () => {
            if (net.wasmReady) {
                c_setRoomCode(net.roomCode);
                if (Module._SetOnlineRole) Module._SetOnlineRole(0);
                if (Module._SetMatchMode) Module._SetMatchMode(2);
            }
            const conn = net.peer.connect(`biro-${net.roomCode}`, { reliable: true });
            setupDataConnection(conn);
        });

        net.peer.on('error', (err) => {
            console.error('[MP] Guest error:', err);
            showToast(`Connection issue: ${err.type}`);
            const badge = document.getElementById('joinStatus');
            if (badge) { badge.textContent = 'Connection failed: ' + err.type; badge.className = 'conn-badge disconnected'; }
        });
    }

    /* ------------------------------------------------------------------
       Data Channel
    ------------------------------------------------------------------ */
    function setupDataConnection(conn) {
        net.conn = conn;

        conn.on('open', () => {
            net.isConnected = true;
            console.log('[MP] DataChannel open!');
            if (window.trackGameEvent) {
                window.trackGameEvent('multiplayer_connected', {
                    role: net.isHost ? 'host' : 'guest',
                    room_code: net.roomCode
                });
            }
            if (net.wasmReady && Module._SetOnlineConnectionStatus) {
                Module._SetOnlineConnectionStatus(1);
            }
            // Update lobby UI
            const badge = document.getElementById('connectionBadge');
            if (badge) { badge.textContent = 'Opponent connected!'; badge.className = 'conn-badge connected'; }
            const joinBadge = document.getElementById('joinStatus');
            if (joinBadge) { joinBadge.textContent = 'Connected! Click Enter Classroom.'; joinBadge.className = 'conn-badge connected'; }

            showToast('Opponent seated at the desk!');
            conn.send({ type: 'handshake', isHost: net.isHost, roomCode: net.roomCode });
        });

        conn.on('data', (data) => handleIncomingPacket(data));

        conn.on('close', () => {
            net.isConnected = false;
            if (net.wasmReady && Module._SetOnlineConnectionStatus) Module._SetOnlineConnectionStatus(0);
            showToast('Opponent left the desk.');
            const badge = document.getElementById('connectionBadge');
            if (badge) { badge.textContent = 'Opponent disconnected'; badge.className = 'conn-badge waiting'; }
        });

        conn.on('error', (err) => console.error('[MP] DataChannel error', err));
    }

    /* ------------------------------------------------------------------
       Packet Handling
    ------------------------------------------------------------------ */
    function handleIncomingPacket(data) {
        if (!data || !data.type) return;
        switch (data.type) {
            case 'strike':
                if (net.wasmReady && Module._ApplyRemoteStrike) {
                    Module._ApplyRemoteStrike(data.player, data.ptX, data.ptY, data.angle, data.power);
                }
                break;
            case 'sync':
                if (net.wasmReady && Module._ApplyRemoteSync) {
                    Module._ApplyRemoteSync(data.player, data.x, data.y, data.angle, data.elim, data.score);
                }
                break;
            case 'restart':
                if (net.wasmReady && Module._RestartMatchFromNetwork) {
                    Module._RestartMatchFromNetwork(data.mode, data.stage, data.table);
                }
                break;
            case 'handshake':
                console.log('[MP] Handshake received', data);
                break;
            case 'chat':
                if (data.text) {
                    const senderName = data.sender === 'P1' ? 'Opponent (P1)' : (data.sender === 'P2' ? 'Opponent (P2)' : 'Classmate');
                    displayDeskNote(senderName, data.text, false);
                }
                break;
        }
    }

    function sendPacket(data) {
        if (net.conn && net.conn.open) net.conn.send(data);
    }

    /* ------------------------------------------------------------------
       UI helper: update the menu room code display + QR
    ------------------------------------------------------------------ */
    function updateRoomCodeUI(code) {
        const el = document.getElementById('displayRoomCode');
        if (el) el.textContent = code;
        const linkEl = document.getElementById('displayRoomLink');
        if (linkEl) linkEl.textContent = getShareableRoomUrl();
        renderQRCode(getShareableRoomUrl());
    }

    /* ------------------------------------------------------------------
       C <-> JS Bridge (called by Raylib/Emscripten via EM_JS)
    ------------------------------------------------------------------ */
    window.onLocalStrike = function(player, ptX, ptY, angle, power) {
        sendPacket({ type: 'strike', player, ptX, ptY, angle, power });
    };

    window.onLocalSync = function(player, x, y, angle, elim, score) {
        if (net.isHost) sendPacket({ type: 'sync', player, x, y, angle, elim, score });
    };

    window.onLocalRoundRestart = function(mode, stage, table) {
        sendPacket({ type: 'restart', mode, stage, table });
    };

    window.copyRoomLink = function() {
        if (!net.roomCode) initHost();
        navigator.clipboard.writeText(getShareableRoomUrl()).then(() => {
            showToast('Invite link copied! Paste it to your friend.');
        }).catch(() => {
            prompt('Copy this Biro Clash invite link:', getShareableRoomUrl());
        });
    };

    window.tweetChallenge = function() {
        if (!net.roomCode) initHost();
        const url  = getShareableRoomUrl();
        const text = `Think your biro skills are elite? Play me in BIRO CLASH on a school desk right now!\n\n${url}\n\n#BiroClash #Box2D #WebAssembly #IndieDev`;
        window.open(`https://twitter.com/intent/tweet?text=${encodeURIComponent(text)}`, '_blank', 'noopener,noreferrer');
    };

    window.initOnlineHost = function() {
        if (!net.roomCode) initHost();
        else c_setRoomCode(net.roomCode);
    };

    /* ------------------------------------------------------------------
       Menu bridge functions (called from index.html inline scripts)
    ------------------------------------------------------------------ */

    // Called when user selects "Online Multiplayer" in the menu
    window.menuInitHost = function() {
        if (!net.peer || net.peer.destroyed) {
            initHost();
        } else {
            updateRoomCodeUI(net.roomCode);
        }
    };

    // Called when user taps "Join Desk" in the menu
    window.menuConnectAsGuest = function(code) {
        net.pendingJoinCode = code.toUpperCase();
        initGuest(net.pendingJoinCode);
    };

    // Returns the shareable URL for the current room
    window.menuGetShareUrl = function() {
        return getShareableRoomUrl();
    };

    // Called by launchGame() in index.html
    window.applyMenuSettings = function() {
        const s = window.menuSel || { mode: 0, matchType: 0, aiDiff: 1, onlineRole: 'host' };

        if (net.wasmReady) {
            c_applyMenuSettings(s);
        } else {
            // Store; will be applied in onWasmInitialized
            net.pendingSettings = s;
        }
    };

    /* ------------------------------------------------------------------
       WASM Runtime Ready callback
    ------------------------------------------------------------------ */
    window.onWasmInitialized = function() {
        console.log('[MP] WASM runtime ready.');
        net.wasmReady = true;

        // Apply any settings chosen in the menu that arrived before WASM was ready
        const s = net.pendingSettings || window.menuSel;
        if (s) {
            c_applyMenuSettings(s);
        }

        // Handle online connection setup
        if (s && s.matchType === 2) {
            if (s.onlineRole === 'join' && net.pendingJoinCode) {
                // Already connecting as guest (was triggered from menuConnectAsGuest)
                c_setRoomCode(net.pendingJoinCode);
                if (Module._SetOnlineRole) Module._SetOnlineRole(0);
                if (Module._SetMatchMode) Module._SetMatchMode(2);
            } else {
                // Host
                if (net.roomCode) {
                    c_setRoomCode(net.roomCode);
                    if (Module._SetOnlineRole) Module._SetOnlineRole(1);
                } else {
                    initHost();
                }
            }
        } else if (!s || s.matchType !== 2) {
            // Non-online: still generate a host code silently so copy-link works
            if (!net.roomCode) initHost();
        }

        // Re-apply connection status if already connected
        if (net.isConnected && Module._SetOnlineConnectionStatus) {
            Module._SetOnlineConnectionStatus(1);
        }
    };

    /* ------------------------------------------------------------------
       Fullscreen
    ------------------------------------------------------------------ */
    window.toggleGameFullscreen = function() {
        const canvas = document.getElementById('canvas');
        if (!document.fullscreenElement) {
            (canvas.requestFullscreen || canvas.webkitRequestFullscreen || canvas.msRequestFullscreen || function(){}).call(canvas);
        } else {
            if (document.exitFullscreen) document.exitFullscreen();
        }
    };

    /* ------------------------------------------------------------------
       Classroom Note Passing (Chatting Functionality)
    ------------------------------------------------------------------ */
    window.toggleDeskNoteDrawer = function() {
        const drawer = document.getElementById('deskNoteDrawer');
        if (!drawer) return;
        const isHidden = drawer.classList.contains('hidden');
        drawer.classList.toggle('hidden', !isHidden);
        if (isHidden) {
            const inp = document.getElementById('chatNoteInput');
            if (inp) {
                setTimeout(() => inp.focus(), 60);
            }
        }
    };

    window.sendQuickNote = function(text) {
        window.sendDeskNote(text);
        const drawer = document.getElementById('deskNoteDrawer');
        if (drawer) drawer.classList.add('hidden');
    };

    window.submitDeskNote = function() {
        const inp = document.getElementById('chatNoteInput');
        if (!inp) return;
        const val = inp.value ? inp.value.trim() : '';
        if (val.length > 0) {
            window.sendDeskNote(val);
            inp.value = '';
        }
        const drawer = document.getElementById('deskNoteDrawer');
        if (drawer) drawer.classList.add('hidden');
    };

    window.sendDeskNote = function(text) {
        if (!text) return;
        const sender = net.isHost ? 'You (P1)' : (net.isConnected ? 'You (P2)' : 'You');
        displayDeskNote(sender, text, true);

        if (window.trackGameEvent) {
            window.trackGameEvent('desk_note_passed', {
                is_multiplayer: !!(net.conn && net.conn.open)
            });
        }

        if (net.conn && net.conn.open) {
            sendPacket({
                type: 'chat',
                sender: net.isHost ? 'P1' : 'P2',
                text: text,
                timestamp: Date.now()
            });
        } else {
            // Solo vs AI Bot banter
            triggerAIBanter(text);
        }
    };

    function triggerAIBanter(userText) {
        const aiReplies = [
            "I calculated this angle using trigonometry! 📐",
            "Watch the desk edge! Don't blame the biro! 🖊️",
            "Lucky bounce off the ruler! 📏",
            "My turn to flick! Don't sneeze! 😤",
            "Shhh! The teacher is looking this way! 🤫",
            "Biro duel of the century! 🔥",
            "Try dodging this bank shot! 🎯",
            "Gravity never sleeps! ⚡"
        ];
        setTimeout(() => {
            const reply = aiReplies[Math.floor(Math.random() * aiReplies.length)];
            displayDeskNote('Classmate AI 🤖', reply, false);
        }, 1300);
    }

    function displayDeskNote(sender, text, isSelf) {
        const el = document.getElementById('deskNoteNotification');
        const senderEl = document.getElementById('deskNoteSender');
        const textEl = document.getElementById('deskNoteText');
        if (!el || !senderEl || !textEl) return;

        senderEl.textContent = sender + ':';
        textEl.textContent = `"${text}"`;
        el.classList.remove('hidden');
        el.classList.remove('anim-fadeout');

        clearTimeout(el._timer);
        el._timer = setTimeout(() => {
            el.classList.add('anim-fadeout');
            setTimeout(() => el.classList.add('hidden'), 350);
        }, 4200);
    }

    // Keyboard shortcut: Press Enter to open or focus Note Passing drawer
    window.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            const drawer = document.getElementById('deskNoteDrawer');
            const inp = document.getElementById('chatNoteInput');
            if (drawer && drawer.classList.contains('hidden')) {
                const gameArea = document.getElementById('gameArea');
                if (gameArea && !gameArea.classList.contains('hidden')) {
                    e.preventDefault();
                    window.toggleDeskNoteDrawer();
                }
            }
        } else if (e.key === 'Escape') {
            const drawer = document.getElementById('deskNoteDrawer');
            if (drawer && !drawer.classList.contains('hidden')) {
                drawer.classList.add('hidden');
            }
        }
    });

})();
