(() => {
  const runtime = window.OneKVMPluginUI.v1
  const { computed, defineComponent, h, onBeforeUnmount, onMounted, ref } = runtime.vue
  const { NAlert, NButton, NInput, NInputNumber, NSpin, NSwitch, NTag, useMessage } = runtime.naive
  const labels = runtime.i18n.createMessages({
    'zh-TW': { title: 'EasyTier', description: '用網路名稱和金鑰加入去中心化組網。', status: '連線狀態', ready: '已設定，等待組網', missing: '尚未填寫網路名稱或金鑰', offline: 'easytier-core 尚未啟動', network: '網路', peers: '公共節點', settings: '連線設定', enabled: '啟用 EasyTier 用戶端', networkName: '網路名稱', networkSecret: '網路金鑰', sharedNodes: '公共節點（每行一個 URI）', dhcp: '自動分配虛擬 IP', ipv4: '虛擬 IPv4', hostname: '主機名稱', listenPort: '監聽連接埠', save: '儲存設定', saved: 'EasyTier 設定已儲存。', refresh: '重新整理' },
    zh: { title: 'EasyTier', description: '用网络名称和密钥加入去中心化组网。', status: '连接状态', ready: '已配置，等待组网', missing: '尚未填写网络名称或密钥', offline: 'easytier-core 尚未启动', network: '网络', peers: '公共节点', settings: '连接设置', enabled: '启用 EasyTier 客户端', networkName: '网络名称', networkSecret: '网络密钥', sharedNodes: '公共节点（每行一个 URI）', dhcp: '自动分配虚拟 IP', ipv4: '虚拟 IPv4', hostname: '主机名', listenPort: '监听端口', save: '保存设置', saved: 'EasyTier 设置已保存。', refresh: '刷新' },
    en: { title: 'EasyTier', description: 'Join a decentralized mesh with a network name and secret.', status: 'Connection status', ready: 'Configured and waiting to join', missing: 'Network name or secret is missing', offline: 'easytier-core is not running', network: 'Network', peers: 'Shared nodes', settings: 'Connection settings', enabled: 'Enable EasyTier client', networkName: 'Network name', networkSecret: 'Network secret', sharedNodes: 'Shared nodes (one URI per line)', dhcp: 'Assign virtual IP automatically', ipv4: 'Virtual IPv4', hostname: 'Hostname', listenPort: 'Listen port', save: 'Save settings', saved: 'EasyTier settings saved.', refresh: 'Refresh' },
  })
  const namePattern = /^[A-Za-z0-9][A-Za-z0-9._-]{0,252}$/
  const ipv4Pattern = /^(?:\d{1,3}\.){3}\d{1,3}(?:\/\d{1,2})?$/

  const Page = defineComponent({
    name: 'EasyTierPage',
    props: { host: { type: Object, required: true } },
    setup(props) {
      const initial = props.host.extension.value.settings
      const serviceEnabled = ref(initial.service_enabled !== false)
      const networkName = ref(String(initial.network_name || ''))
      const networkSecret = ref(String(initial.network_secret || ''))
      const peers = ref(String(initial.peers || 'tcp://public.easytier.cn:11010').replace(/[,\s]+/g, '\n').trim())
      const dhcp = ref(initial.dhcp !== false)
      const ipv4 = ref(String(initial.ipv4 || ''))
      const hostname = ref(String(initial.hostname || 'onekvm'))
      const listenPort = ref(Number(initial.listen_port ?? 11010))
      const status = ref({ ready: false, networkName: '', dhcp: true, ipv4: '', hostname: 'onekvm', listenPort: 11010, peers: [] })
      const loading = ref(false)
      const busy = ref('')
      const error = ref('')
      const saved = ref('')
      const message = useMessage()
      let timer = null

      const snapshot = () => JSON.stringify([serviceEnabled.value, networkName.value, networkSecret.value, peers.value, dhcp.value, ipv4.value, hostname.value, listenPort.value])
      saved.value = snapshot()
      const dirty = computed(() => snapshot() !== saved.value)
      const valid = computed(() => networkName.value.length > 0 && networkName.value.length <= 64 &&
        networkSecret.value.length > 0 && networkSecret.value.length <= 4096 &&
        peers.value.trim().length > 0 && peers.value.length <= 1024 &&
        hostname.value.length <= 253 && (hostname.value.length === 0 || namePattern.test(hostname.value)) &&
        Number.isInteger(listenPort.value) && listenPort.value >= 1 && listenPort.value <= 65535 &&
        (dhcp.value || (ipv4.value.length > 0 && ipv4Pattern.test(ipv4.value))))

      const unregister = props.host.registerSettings({
        dirty,
        valid,
        showHostActions: false,
        collect: () => ({
          service_enabled: serviceEnabled.value,
          network_name: networkName.value,
          network_secret: networkSecret.value,
          peers: peers.value.trim().split(/[\s,;]+/).filter(Boolean).join('\n'),
          dhcp: dhcp.value,
          ipv4: ipv4.value,
          hostname: hostname.value,
          listen_port: listenPort.value,
        }),
        reset: (settings) => {
          serviceEnabled.value = settings.service_enabled !== false
          networkName.value = String(settings.network_name || '')
          networkSecret.value = String(settings.network_secret || '')
          peers.value = String(settings.peers || 'tcp://public.easytier.cn:11010').replace(/[,\s]+/g, '\n').trim()
          dhcp.value = settings.dhcp !== false
          ipv4.value = String(settings.ipv4 || '')
          hostname.value = String(settings.hostname || 'onekvm')
          listenPort.value = Number(settings.listen_port ?? 11010)
          saved.value = snapshot()
        },
      })

      async function refresh(showSpinner = false) {
        if (loading.value) return
        loading.value = true
        if (showSpinner) busy.value = 'refresh'
        try {
          const next = await props.host.invoke('status')
          status.value = next && typeof next === 'object' && next.response ? next.response : (next || status.value)
          error.value = ''
        } catch (reason) {
          error.value = reason instanceof Error ? reason.message : String(reason)
        } finally {
          loading.value = false
          if (busy.value === 'refresh') busy.value = ''
        }
      }

      async function saveSettings() {
        if (!valid.value || busy.value) return
        busy.value = 'save'
        try {
          if (await props.host.saveSettings()) {
            saved.value = snapshot()
            message.success(labels.saved)
            await refresh()
          }
        } finally {
          busy.value = ''
        }
      }

      onMounted(() => {
        void refresh(true)
        timer = window.setInterval(() => { void refresh(false) }, 4000)
      })
      onBeforeUnmount(() => {
        unregister()
        if (timer !== null) window.clearInterval(timer)
      })

      return () => h('section', { class: 'ok-page' }, [
        h('div', { class: 'ok-heading' }, [
          h('p', labels.description),
          h(NButton, { secondary: true, size: 'small', loading: busy.value === 'refresh', disabled: Boolean(busy.value && busy.value !== 'refresh'), onClick: () => refresh(true) }, { default: () => labels.refresh }),
        ]),
        error.value ? h(NAlert, { type: 'error', showIcon: false }, { default: () => error.value }) : null,
        h(NSpin, { show: loading.value && busy.value === 'refresh' }, { default: () => h('article', { class: 'ok-card ok-status' }, [
          h(NTag, { size: 'small', type: status.value.ready ? 'success' : 'warning', bordered: false }, { default: () => status.value.ready ? labels.ready : labels.missing }),
          status.value.networkName ? h('span', `${labels.network}: ${status.value.networkName}`) : h('span', { class: 'ok-muted' }, labels.offline),
          Array.isArray(status.value.peers) && status.value.peers.length ? h('small', `${labels.peers}: ${status.value.peers.join(', ')}`) : null,
        ]) }),
        h('article', { class: 'ok-card' }, [
          h('strong', labels.settings),
          h('label', { class: 'ok-switch' }, [h('span', labels.enabled), h(NSwitch, { value: serviceEnabled.value, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { serviceEnabled.value = value } })]),
          h('div', { class: 'ok-grid' }, [
            h('label', [h('span', labels.networkName), h(NInput, { value: networkName.value, maxlength: 64, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { networkName.value = value } })]),
            h('label', [h('span', labels.networkSecret), h(NInput, { value: networkSecret.value, type: 'password', showPasswordOn: 'click', maxlength: 4096, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { networkSecret.value = value } })]),
            h('label', { class: 'ok-span' }, [h('span', labels.sharedNodes), h(NInput, { value: peers.value, type: 'textarea', rows: 3, maxlength: 1024, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { peers.value = value } })]),
            h('label', [h('span', labels.dhcp), h(NSwitch, { value: dhcp.value, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { dhcp.value = value } })]),
            h('label', [h('span', labels.ipv4), h(NInput, { value: ipv4.value, maxlength: 18, disabled: Boolean(busy.value) || dhcp.value, 'onUpdate:value': (value) => { ipv4.value = value } })]),
            h('label', [h('span', labels.hostname), h(NInput, { value: hostname.value, maxlength: 253, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { hostname.value = value } })]),
            h('label', [h('span', labels.listenPort), h(NInputNumber, { value: listenPort.value, min: 1, max: 65535, disabled: Boolean(busy.value), 'onUpdate:value': (value) => { listenPort.value = value ?? 11010 } })]),
          ]),
          h('div', { class: 'ok-save' }, [h(NButton, { type: 'primary', loading: busy.value === 'save', disabled: !dirty.value || !valid.value || Boolean(busy.value && busy.value !== 'save'), onClick: saveSettings }, { default: () => labels.save })]),
        ]),
      ])
    },
  })

  runtime.register('easytier', {
    apiVersion: 1,
    component: Page,
    styles: [`
      .ok-page { display: grid; gap: 10px; }
      .ok-heading { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
      .ok-heading p { margin: 0; color: var(--n-text-color-3, #888); font-size: 13px; line-height: 1.4; }
      .ok-card { display: grid; gap: 12px; padding: 14px; border: 1px solid var(--n-border-color, #444); border-radius: 8px; }
      .ok-card.ok-status { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 16px; min-height: 36px; padding: 6px 12px; border-radius: 6px; }
      .ok-status-title { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
      .ok-muted, .ok-card small { color: var(--n-text-color-3, #888); }
      .ok-grid { display: grid; grid-template-columns: minmax(0, 1fr); gap: 13px; }
      .ok-grid > label { display: grid; gap: 7px; min-width: 0; }
      .ok-grid .n-switch { justify-self: start; }
      .ok-span { grid-column: 1 / -1; }
      .ok-switch { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
      .ok-save { display: flex; justify-content: flex-end; }
      @media (min-width: 720px) { .ok-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
      @media (max-width: 719px) { .ok-heading { flex-direction: column; } }
    `],
  })
})()
