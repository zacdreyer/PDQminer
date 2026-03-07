/* PDQManager – Client-side logic */

/**
 * Format seconds into a human-readable uptime string.
 * @param {number} seconds
 * @returns {string}
 */
function formatUptime(seconds) {
    const d = Math.floor(seconds / 86400);
    const h = Math.floor((seconds % 86400) / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    if (d > 0) return `${d}d ${h}h ${m}m`;
    if (h > 0) return `${h}h ${m}m`;
    return `${m}m`;
}

/**
 * Refresh the device list from the API.
 */
async function refreshDevices() {
    try {
        const resp = await fetch('/api/devices');
        if (!resp.ok) return;
        const devices = await resp.json();
        renderDeviceList(devices);
    } catch (e) {
        /* silently ignore polling failures */
    }
}

/**
 * Refresh fleet statistics from the API.
 */
async function refreshStats() {
    try {
        const resp = await fetch('/api/stats');
        if (!resp.ok) return;
        const stats = await resp.json();

        const el = (id) => document.getElementById(id);
        if (el('stat-total')) el('stat-total').textContent = stats.total_devices;
        if (el('stat-online')) el('stat-online').textContent = stats.online_devices;
        if (el('stat-hashrate')) el('stat-hashrate').textContent = stats.total_hashrate_khs.toFixed(1) + ' KH/s';
        if (el('stat-shares')) el('stat-shares').textContent = stats.total_shares_accepted;
    } catch (e) {
        /* silently ignore */
    }
}

/**
 * Trigger a network scan.
 */
async function scanNetwork() {
    try {
        await fetch('/api/scan', { method: 'POST' });
        await refreshDevices();
        await refreshStats();
    } catch (e) {
        /* ignore */
    }
}

/**
 * Render the device card grid.
 * @param {Array} devices
 */
function renderDeviceList(devices) {
    const container = document.getElementById('device-list');
    if (!container) return;

    if (!devices || devices.length === 0) {
        container.innerHTML = '<p class="muted">No devices discovered yet. Click "Scan Network" to search.</p>';
        return;
    }

    container.innerHTML = devices.map(d => {
        const id = d.device_id || 'unknown';
        const ip = d.ip_address || d.ip || '';
        const hr = d.hashrate_khs != null ? d.hashrate_khs.toFixed(1) : '0.0';
        const online = d.pool_connected;
        const statusClass = online ? 'online' : 'offline';
        const statusText = online ? 'Online' : 'Offline';
        const shares = (d.shares_accepted || 0) + ' shares';

        return `
            <div class="device-card" onclick="location.href='/device/${encodeURIComponent(id)}'">
                <h3>${escapeHtml(id)}</h3>
                <div class="device-ip">${escapeHtml(ip)}</div>
                <div class="device-hr">${hr} KH/s</div>
                <div class="device-status ${statusClass}">${statusText} &middot; ${shares}</div>
            </div>
        `;
    }).join('');
}

/**
 * Escape HTML to prevent XSS.
 * @param {string} text
 * @returns {string}
 */
function escapeHtml(text) {
    const el = document.createElement('span');
    el.textContent = text;
    return el.innerHTML;
}
