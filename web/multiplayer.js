/**
 * ============================================================================
 * BIRO CLASH: School Desk Physics
 * WebRTC PeerJS Zero-Cost Multiplayer & Viral Social Bridge
 * ============================================================================
 * 
 * 100% Free Forever Architecture:
 * - Peer-to-peer WebRTC DataChannels (zero game server data transmission)
 * - Public free cloud broker (0.peerjs.com) for initial SDP signaling exchange
 * - Google Public STUN servers for NAT traversal
 * - Host / Guest peer architecture with automatic URL query room joining
 */

(function () {
    'use strict';

    // Game Network State
    const net = {
        peer: null,
        conn: null,
        roomCode: '',
        fullPeerId: '',
        isHost: true,
        isConnected: false,
        wasmReady: false,
        pendingJoinCode: null
    };

    // Public STUN server configuration for global peer connectivity
    const PEER_CONFIG = {
        debug: 1,
        config: {
            iceServers: [
                { urls: 'stun:stun.l.google.com:19302' },
                { urls: 'stun:stun1.l.google.com:19302' },
                { urls: 'stun:stun2.l.google.com:19302' },
                { urls: 'stun:stun.services.mozilla.com' }
            ]
        }
    };

    // Generate random 5-character readable classroom room code (no ambiguous 0/O or 1/I)
    function generateRoomCode() {
        const chars = '23456789ABCDEFGHJKLMNPQRSTUVWXYZ';
        let code = '';
        for (let i = 0; i < 5; i++) {
            code += chars.charAt(Math.floor(Math.random() * chars.length));
        }
        return code;
    }

    // Helper to pass JS string to Emscripten C char*
    function c_setRoomCode(code) {
        if (!window.Module || !Module._SetOnlineRoomCode) return;
        if (typeof Module.stringToUTF8 === 'function' && typeof Module._malloc === 'function') {
            const lengthBytes = (code.length * 4) + 1;
            const ptr = Module._malloc(lengthBytes);
            Module.stringToUTF8(code, ptr, lengthBytes);
            Module._SetOnlineRoomCode(ptr);
            Module._free(ptr);
        } else if (typeof Module.allocateUTF8 === 'function') {
            const ptr = Module.allocateUTF8(code);
            Module._SetOnlineRoomCode(ptr);
            if (Module._free) Module._free(ptr);
        }
    }

    // Show temporary floating school toast notification
    function showToast(text, durationMs = 3500) {
        let toast = document.getElementById('toastNotice');
        if (!toast) {
            toast = document.createElement('div');
            toast.id = 'toastNotice';
            toast.className = 'toast-notice';
            document.body.appendChild(toast);
        }
        toast.innerHTML = `<span class="toast-icon">📋</span> <span>${text}</span>`;
        toast.classList.add('show');
        clearTimeout(toast._timeout);
        toast._timeout = setTimeout(() => {
            toast.classList.remove('show');
        }, durationMs);
    }

    // Construct the direct shareable room URL
    function getShareableRoomUrl() {
        const url = new URL(window.location.href);
        url.search = `?room=${net.roomCode}`;
        return url.toString();
    }

    // Initialize Host Peer
    function initHost(code = null) {
        if (net.peer && !net.peer.destroyed) {
            if (net.roomCode) return;
        }

        net.isHost = true;
        net.roomCode = (code || generateRoomCode()).toUpperCase();
        net.fullPeerId = `biro-${net.roomCode}`;

        console.log(`[Multiplayer] Initializing Host Peer: ${net.fullPeerId}`);

        try {
            net.peer = new Peer(net.fullPeerId, PEER_CONFIG);
        } catch (e) {
            console.error('[Multiplayer] Failed to create PeerJS host instance:', e);
            return;
        }

        net.peer.on('open', (id) => {
            console.log(`[Multiplayer] Host Peer Open with ID: ${id}`);
            if (net.wasmReady) {
                c_setRoomCode(net.roomCode);
                if (Module._SetOnlineRole) Module._SetOnlineRole(1); // Host
            }
        });

        net.peer.on('connection', (connection) => {
            console.log('[Multiplayer] Opponent guest connected!');
            setupDataConnection(connection);
        });

        net.peer.on('error', (err) => {
            console.warn('[Multiplayer] PeerJS error:', err.type, err);
            if (err.type === 'unavailable-id') {
                // Code collision, retry with a fresh code
                console.log('[Multiplayer] Room code collision, generating fresh code...');
                net.peer.destroy();
                initHost();
            }
        });
    }

    // Initialize Guest Peer and join a host room
    function initGuest(targetCode) {
        net.isHost = false;
        net.roomCode = targetCode.toUpperCase();
        net.fullPeerId = `biro-guest-${generateRoomCode()}`;

        console.log(`[Multiplayer] Initializing Guest Peer to join room: ${net.roomCode}`);

        try {
            net.peer = new Peer(net.fullPeerId, PEER_CONFIG);
        } catch (e) {
            console.error('[Multiplayer] Failed to create PeerJS guest instance:', e);
            return;
        }

        net.peer.on('open', (id) => {
            console.log(`[Multiplayer] Guest Peer Open (${id}), connecting to biro-${net.roomCode}...`);
            if (net.wasmReady) {
                c_setRoomCode(net.roomCode);
                if (Module._SetOnlineRole) Module._SetOnlineRole(0); // Guest
                if (Module._SetMatchMode) Module._SetMatchMode(2);  // MATCH_ONLINE_P2P
            }

            const targetPeerId = `biro-${net.roomCode}`;
            const conn = net.peer.connect(targetPeerId, { reliable: true });
            setupDataConnection(conn);
        });

        net.peer.on('error', (err) => {
            console.error('[Multiplayer] Guest Peer error:', err);
            showToast(`⚠️ Connection issue: ${err.type}`);
        });
    }

    // Configure WebRTC DataChannel callbacks
    function setupDataConnection(conn) {
        net.conn = conn;

        conn.on('open', () => {
            console.log('[Multiplayer] DataChannel Connected!');
            net.isConnected = true;

            if (net.wasmReady && Module._SetOnlineConnectionStatus) {
                Module._SetOnlineConnectionStatus(1);
            }

            showToast('🎉 OPPONENT CONNECTED! DESK MATCH READY!', 4000);

            // Send handshake
            conn.send({
                type: 'handshake',
                isHost: net.isHost,
                roomCode: net.roomCode
            });
        });

        conn.on('data', (data) => {
            handleIncomingPacket(data);
        });

        conn.on('close', () => {
            console.log('[Multiplayer] DataChannel Closed');
            net.isConnected = false;
            if (net.wasmReady && Module._SetOnlineConnectionStatus) {
                Module._SetOnlineConnectionStatus(0);
            }
            showToast('⚠️ Opponent left the desk.', 4000);
        });

        conn.on('error', (err) => {
            console.error('[Multiplayer] DataChannel error:', err);
        });
    }

    // Process incoming network packets
    function handleIncomingPacket(data) {
        if (!data || !data.type) return;

        switch (data.type) {
            case 'strike':
                if (net.wasmReady && Module._ApplyRemoteStrike) {
                    console.log(`[Multiplayer] Received Remote Strike: P${data.player + 1} angle=${data.angle.toFixed(2)} pow=${data.power.toFixed(2)}`);
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
                    console.log(`[Multiplayer] Received Match Restart (mode=${data.mode}, stage=${data.stage}, table=${data.table})`);
                    Module._RestartMatchFromNetwork(data.mode, data.stage, data.table);
                }
                break;

            case 'handshake':
                console.log('[Multiplayer] Received Handshake from opponent', data);
                break;
        }
    }

    // Safe packet sender
    function sendPacket(data) {
        if (net.conn && net.conn.open) {
            net.conn.send(data);
        }
    }

    // =========================================================================
    // C <-> JS Bridge Functions (Called by Raylib C code via EM_JS)
    // =========================================================================

    // Broadcast a flick strike to peer
    window.onLocalStrike = function (player, ptX, ptY, angle, power) {
        sendPacket({
            type: 'strike',
            player: player,
            ptX: ptX,
            ptY: ptY,
            angle: angle,
            power: power
        });
    };

    // Broadcast table settle synchronization to peer
    window.onLocalSync = function (player, x, y, angle, elim, score) {
        // Only host sends definitive sync updates to prevent oscillation
        if (net.isHost) {
            sendPacket({
                type: 'sync',
                player: player,
                x: x,
                y: y,
                angle: angle,
                elim: elim,
                score: score
            });
        }
    };

    // Broadcast round restart
    window.onLocalRoundRestart = function (mode, stage, table) {
        sendPacket({
            type: 'restart',
            mode: mode,
            stage: stage,
            table: table
        });
    };

    // Copy room link to clipboard
    window.copyRoomLink = function () {
        if (!net.roomCode) {
            initHost();
        }
        const inviteUrl = getShareableRoomUrl();
        navigator.clipboard.writeText(inviteUrl).then(() => {
            showToast('📋 INVITE LINK COPIED! Paste to friend on Twitter / WhatsApp!');
        }).catch(() => {
            prompt('Copy this Biro Clash invite link:', inviteUrl);
        });
    };

    // Launch Twitter / X challenge tweet intent
    window.tweetChallenge = function () {
        if (!net.roomCode) {
            initHost();
        }
        const inviteUrl = getShareableRoomUrl();
        const tweetText = `⚡ Think your biro flicking skills are elite? Play me in BIRO CLASH on a school desk right now in your browser!\n\n🎮 1-Click P2P Match: ${inviteUrl}\n\n#BiroClash #Box2D #WebAssembly #IndieDev`;
        const tweetUrl = `https://twitter.com/intent/tweet?text=${encodeURIComponent(tweetText)}`;
        window.open(tweetUrl, '_blank', 'noopener,noreferrer');
    };

    // Request room code when user enters online mode
    window.initOnlineHost = function () {
        if (!net.roomCode) {
            initHost();
        } else {
            c_setRoomCode(net.roomCode);
        }
    };

    // Initialize multiplayer on page load
    window.addEventListener('DOMContentLoaded', () => {
        const urlParams = new URLSearchParams(window.location.search);
        const room = urlParams.get('room');

        if (room && room.trim().length > 0) {
            net.pendingJoinCode = room.trim().toUpperCase();
            console.log(`[Multiplayer] Detected room code in URL: ${net.pendingJoinCode}`);
            showToast(`Joining desk duel room ${net.pendingJoinCode}...`);
        }
    });

    // Hook called when WebAssembly runtime has finished compiling & loading
    window.onWasmInitialized = function () {
        console.log('[Multiplayer] WASM runtime ready.');
        net.wasmReady = true;

        if (net.pendingJoinCode) {
            initGuest(net.pendingJoinCode);
        } else {
            // Default setup: generate host room code ready for sharing
            initHost();
        }
    };

    // Global toggle fullscreen helper
    window.toggleGameFullscreen = function () {
        const canvas = document.getElementById('canvas');
        if (!document.fullscreenElement) {
            if (canvas.requestFullscreen) canvas.requestFullscreen();
            else if (canvas.webkitRequestFullscreen) canvas.webkitRequestFullscreen();
            else if (canvas.msRequestFullscreen) canvas.msRequestFullscreen();
        } else {
            if (document.exitFullscreen) document.exitFullscreen();
        }
    };

})();
