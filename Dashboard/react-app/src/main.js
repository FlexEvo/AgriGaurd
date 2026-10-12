import React, { useCallback, useEffect, useState } from "https://esm.sh/react@18.3.1";
import { createRoot } from "https://esm.sh/react-dom@18.3.1/client";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.57.0";

const h = React.createElement;
const SUPABASE_URL = "https://jxotmuxfctrovjsfhrmm.supabase.co";
const SUPABASE_PUBLISHABLE_KEY = "sb_publishable_0x_DDCbnYZFuLG7bKDXKVg_v6k71QwX";
const supabase = createClient(SUPABASE_URL, SUPABASE_PUBLISHABLE_KEY, {
  auth: { persistSession: true, autoRefreshToken: true, detectSessionInUrl: true }
});

function App() {
  const [session, setSession] = useState(null);
  const [page, setPage] = useState("login");
  const [busy, setBusy] = useState(false);
  const [notice, setNotice] = useState("");
  const [error, setError] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [fullName, setFullName] = useState("");
  const [farmName, setFarmName] = useState("");
  const [location, setLocation] = useState("");
  const [tankCapacity, setTankCapacity] = useState("5250");
  const [zonesInput, setZonesInput] = useState([
    { name: "Zone 1", soil_type: "loamy" },
    { name: "Zone 2", soil_type: "clay" },
    { name: "Zone 3", soil_type: "sandy" }
  ]);
  const [farm, setFarm] = useState(null);
  const [zones, setZones] = useState([]);
  const [readings, setReadings] = useState([]);
  const [irrigationCycles, setIrrigationCycles] = useState([]);
  const [tank, setTank] = useState(null);
  const [device, setDevice] = useState(null);
  const [lastUpdated, setLastUpdated] = useState(null);
  const [loadingData, setLoadingData] = useState(false);

  useEffect(() => {
    supabase.auth.getSession().then(({ data }) => {
      setSession(data.session);
      setPage(data.session ? "dashboard" : "login");
    });
    const { data: listener } = supabase.auth.onAuthStateChange((_event, nextSession) => {
      setSession(nextSession);
      setPage(nextSession ? "dashboard" : "login");
    });
    return () => listener.subscription.unsubscribe();
  }, []);

  const loadDashboard = useCallback(async (quiet = false) => {
    if (!session?.user) return;
    if (!quiet) setLoadingData(true);
    setError("");
    try {
      const { data: farms, error: farmError } = await supabase
        .from("farms").select("id,name,location_name,tank_capacity_l,tank_reserve_percent,water_budget_l,created_at")
        .eq("owner_id", session.user.id).order("created_at", { ascending: false }).limit(1);
      if (farmError) throw farmError;
      const selectedFarm = farms?.[0] ?? null;
      setFarm(selectedFarm);
      if (!selectedFarm) {
        setZones([]); setReadings([]); setIrrigationCycles([]); setTank(null); setDevice(null);
        setPage("setup");
        return;
      }
      const { data: zoneRows, error: zoneError } = await supabase
        .from("zones").select("id,zone_number,name,soil_type,moisture_start_threshold,moisture_stop_threshold,target_moisture_percent")
        .eq("farm_id", selectedFarm.id).order("zone_number");
      if (zoneError) throw zoneError;
      setZones(zoneRows ?? []);
      const { data: deviceRows, error: deviceError } = await supabase
        .from("devices").select("id,name,device_uid,device_type,is_active,last_seen_at,firmware_version")
        .eq("farm_id", selectedFarm.id).order("created_at", { ascending: false }).limit(1);
      if (deviceError) throw deviceError;
      const selectedDevice = deviceRows?.[0] ?? null;
      setDevice(selectedDevice);
      if (selectedDevice) {
        const [sensorResult, tankResult] = await Promise.all([
          supabase.from("sensor_readings")
            .select("id,zone_id,moisture_percent,raw_value,temperature_c,humidity_percent,drying_rate,recorded_at")
            .eq("device_id", selectedDevice.id).order("recorded_at", { ascending: false }).limit(60),
          supabase.from("tank_readings")
            .select("level_percent,volume_l,distance_cm,recorded_at")
            .eq("device_id", selectedDevice.id).order("recorded_at", { ascending: false }).limit(1)
        ]);
        if (sensorResult.error) throw sensorResult.error;
        if (tankResult.error) throw tankResult.error;
        setReadings(sensorResult.data ?? []);
        setTank(tankResult.data?.[0] ?? null);
      } else {
        setReadings([]); setTank(null);
      }
      const { data: cycleRows, error: cycleError } = await supabase
        .from("irrigation_cycles")
        .select("id,zone_id,started_at,ended_at,duration_seconds,moisture_before_percent,moisture_after_percent,water_used_l,trigger_reason,status,cycle_number")
        .eq("farm_id", selectedFarm.id)
        .order("started_at", { ascending: false })
        .limit(25);
      if (cycleError) throw cycleError;
      setIrrigationCycles(cycleRows ?? []);
      setLastUpdated(new Date());
    } catch (e) {
      setError(e?.message || "Could not load farm data. Check database permissions and connection.");
    } finally {
      setLoadingData(false);
    }
  }, [session]);

  useEffect(() => {
    if (!session) return;
    loadDashboard();
    const timer = setInterval(() => loadDashboard(true), 15000);
    return () => clearInterval(timer);
  }, [session, loadDashboard]);

  async function submitAuth(event) {
    event.preventDefault(); setBusy(true); setError(""); setNotice("");
    try {
      if (page === "signup") {
        const { data, error: e } = await supabase.auth.signUp({
          email: email.trim(), password,
          options: { data: { full_name: fullName.trim() } }
        });
        if (e) throw e;
        if (!data.session) {
          setNotice("Account request received. Check your email to confirm your address, then sign in.");
          setPage("login");
        } else {
          setSession(data.session); setPage("setup");
        }
      } else {
        const { data, error: e } = await supabase.auth.signInWithPassword({ email: email.trim(), password });
        if (e) throw e;
        setSession(data.session); setPage("dashboard");
      }
    } catch (e) { setError(e?.message || "Authentication failed."); }
    finally { setBusy(false); }
  }

  function resizeZones(count) {
    const safeCount = Math.max(1, Math.min(12, Number(count) || 1));
    setZonesInput(old => Array.from({ length: safeCount }, (_, i) => old[i] || { name: "Zone " + (i + 1), soil_type: "loamy" }));
  }

  async function createFarm(event) {
    event.preventDefault(); setBusy(true); setError(""); setNotice("");
    try {
      const { data: auth } = await supabase.auth.getUser();
      if (!auth.user) throw new Error("Your session expired. Please sign in again.");
      const capacity = Number(tankCapacity);
      if (!farmName.trim() || !Number.isFinite(capacity) || capacity < 1) throw new Error("Enter a farm name and a valid tank capacity.");
      const { data: newFarm, error: e } = await supabase.from("farms").insert({
        owner_id: auth.user.id, name: farmName.trim(),
        location_name: location.trim() || null, tank_capacity_l: capacity,
        tank_reserve_percent: 30
      }).select("id,name,location_name,tank_capacity_l,tank_reserve_percent,water_budget_l,created_at").single();
      if (e) throw e;
      const zonePayload = zonesInput.map((z, i) => ({
        farm_id: newFarm.id, zone_number: i + 1, name: z.name.trim() || `Zone ${i + 1}`,
        soil_type: z.soil_type, moisture_start_threshold: z.soil_type === "clay" ? 47 : z.soil_type === "sandy" ? 27 : 40,
        moisture_stop_threshold: z.soil_type === "clay" ? 70 : z.soil_type === "sandy" ? 48 : 56
      }));
      const { error: zoneError } = await supabase.from("zones").insert(zonePayload);
      if (zoneError) throw zoneError;
      setFarm(newFarm); setNotice("Farm profile saved."); setPage("dashboard");
      await loadDashboard(true);
    } catch (e) { setError(e?.message || "Could not save the farm. Check your Supabase policies."); }
    finally { setBusy(false); }
  }

  async function signOut() {
    const { error: e } = await supabase.auth.signOut();
    if (e) setError(e.message);
    setSession(null); setFarm(null); setPage("login");
  }

  function field(label, value, onChange, props = {}) {
    return h("label", { className: "field", key: label },
      h("span", null, label),
      h("input", { value, onChange: e => onChange(e.target.value), ...props })
    );
  }

  function authView() {
    const signup = page === "signup";
    return h("div", { className: "auth-layout" },
      h("section", { className: "auth-aside" },
        h("div", { className: "brand light" }, h("span", { className: "brand-icon" }, "🌱"), "Agriguard"),
        h("div", { className: "aside-copy" },
          h("p", { className: "eyebrow" }, "SMART IRRIGATION"),
          h("h1", null, "Grow more. Waste less water."),
          h("p", null, "A clearer picture of your soil, water and irrigation system — built for farmers.")
        ),
        h("p", { className: "aside-foot" }, "Your farm data, connected in one place.")
      ),
      h("section", { className: "auth-main" },
        h("form", { className: "auth-card", onSubmit: submitAuth },
          h("p", { className: "eyebrow" }, signup ? "GET STARTED" : "WELCOME BACK"),
          h("h2", null, signup ? "Create your account" : "Sign in to Agriguard"),
          h("p", { className: "muted intro" }, signup ? "Set up your farmer account to begin." : "Access your farm dashboard and latest sensor readings."),
          signup ? field("Full name", fullName, setFullName, { autoComplete: "name", required: true }) : null,
          field("Email address", email, setEmail, { type: "email", autoComplete: "email", required: true }),
          field("Password", password, setPassword, { type: "password", autoComplete: signup ? "new-password" : "current-password", minLength: 8, required: true }),
          error ? h("div", { className: "message error", role: "alert" }, error) : null,
          notice ? h("div", { className: "message success", role: "status" }, notice) : null,
          h("button", { className: "button primary wide", type: "submit", disabled: busy }, busy ? "Please wait…" : signup ? "Create account" : "Sign in"),
          h("p", { className: "switch-auth" }, signup ? "Already registered? " : "New to Agriguard? ",
            h("button", { className: "text-button", type: "button", onClick: () => { setError(""); setNotice(""); setPage(signup ? "login" : "signup"); } }, signup ? "Sign in" : "Create an account")
          ),
          h("p", { className: "fine-print" }, "Use a unique password. Your account is managed securely by Supabase Auth.")
        )
      )
    );
  }

  function setupView() {
    return h("div", { className: "setup-page" },
      h("header", { className: "topbar" }, h("div", { className: "brand" }, h("span", { className: "brand-icon" }, "🌱"), "Agriguard"),
        h("button", { className: "button ghost", onClick: signOut }, "Sign out")),
      h("form", { className: "setup-card", onSubmit: createFarm },
        h("p", { className: "eyebrow" }, "FARM SETUP"),
        h("h1", null, "Tell us about your farm"),
        h("p", { className: "muted intro" }, "These details personalise your dashboard. You can refine zone settings later."),
        field("Farm name", farmName, setFarmName, { placeholder: "e.g. Green Valley Farm", required: true }),
        field("Location (optional)", location, setLocation, { placeholder: "Town, district or village" }),
        h("label", { className: "field" }, h("span", null, "Tank capacity (litres)"),
          h("select", { value: tankCapacity, onChange: e => setTankCapacity(e.target.value) },
            ...[2000,5250,5500,10000,15000,20000].map(n => h("option", { value: String(n), key: n }, n.toLocaleString() + " L")))),
        h("div", { className: "section-heading zone-count-heading" },
          h("div", null, h("h2", null, "Irrigation zones"), h("p", { className: "muted" }, "Choose how many zones your farm has, then set each zone name and soil type.")),
          h("label", { className: "field zone-count-field" }, h("span", null, "Number of zones"),
            h("select", { value: String(zonesInput.length), onChange: e => resizeZones(e.target.value) },
              ...Array.from({ length: 12 }, (_, i) => i + 1).map(n => h("option", { value: String(n), key: n }, n + (n === 1 ? " zone" : " zones")))))),
        ...zonesInput.map((z, i) => h("div", { className: "zone-setup", key: i },
          h("label", { className: "field" }, h("span", null, `Zone ${i + 1} name`),
            h("input", { value: z.name, onChange: e => setZonesInput(old => old.map((v, j) => j === i ? { ...v, name: e.target.value } : v)), required: true })),
          h("label", { className: "field" }, h("span", null, "Soil type"),
            h("select", { value: z.soil_type, onChange: e => setZonesInput(old => old.map((v, j) => j === i ? { ...v, soil_type: e.target.value } : v)) },
              h("option", { value: "loamy" }, "Loamy"), h("option", { value: "clay" }, "Clay"), h("option", { value: "sandy" }, "Sandy")))
        )),
        h("div", { className: "reserve-note" }, h("strong", null, "30% protected water reserve"),
          h("p", null, "Agriguard records this reserve as a farm setting. This page does not remotely start irrigation."),
        ),
        error ? h("div", { className: "message error", role: "alert" }, error) : null,
        h("button", { className: "button primary wide", type: "submit", disabled: busy }, busy ? "Saving farm…" : "Save farm and open dashboard")
      )
    );
  }

  function dashboardView() {
    const latestByZone = new Map();
    readings.forEach(r => { if (r.zone_id && !latestByZone.has(r.zone_id)) latestByZone.set(r.zone_id, r); });
    const online = !!device?.last_seen_at && (Date.now() - new Date(device.last_seen_at).getTime() < 120000);
    const capacity = Number(farm?.tank_capacity_l || 0);
    const tankPct = tank?.level_percent == null ? null : Number(tank.level_percent);
    return h("div", { className: "app-shell" },
      h("aside", { className: "sidebar" },
        h("div", { className: "brand" }, h("span", { className: "brand-icon" }, "🌱"), "Agriguard"),
        h("div", { className: "nav-label" }, "FARM"),
        h("div", { className: "nav-item active" }, h("span", null, "▦"), "Overview"),
        h("div", { className: "nav-item" }, h("span", null, "◉"), "Soil zones"),
        h("div", { className: "nav-item" }, h("span", null, "◷"), "Irrigation history"),
        h("div", { className: "sidebar-bottom" }, h("div", { className: "mini-status" }, h("span", { className: "status-dot " + (online ? "online" : "offline") }), online ? "Controller connected" : "Waiting for controller"),
          h("button", { className: "button ghost wide", onClick: signOut }, "Sign out"))
      ),
      h("main", { className: "dashboard-main" },
        h("header", { className: "dashboard-header" },
          h("div", null, h("p", { className: "eyebrow" }, "FARM OVERVIEW"), h("h1", null, farm?.name || "Your farm"),
            h("p", { className: "muted" }, farm?.location_name || "Location not set")),
          h("div", { className: "header-actions" },
            h("span", { className: "live-pill" }, h("span", { className: "status-dot " + (online ? "online" : "offline") }), online ? "Device online" : "Device not recently seen"),
            h("button", { className: "button ghost", onClick: () => loadDashboard() }, loadingData ? "Refreshing…" : "↻ Refresh"))
        ),
        notice ? h("div", { className: "message success" }, notice) : null,
        error ? h("div", { className: "message error", role: "alert" }, error) : null,
        h("section", { className: "metric-grid" },
          metric("Tank level", tankPct == null ? "No reading" : tankPct.toFixed(1) + "%", tank?.volume_l != null ? Number(tank.volume_l).toLocaleString() + " L recorded" : capacity ? `Tank capacity ${capacity.toLocaleString()} L` : "Configure tank capacity", "◒"),
          metric("Soil zones", String(zones.length), `${latestByZone.size} zone(s) with readings`, "♧"),
          metric("Latest telemetry", readings[0] ? formatDate(readings[0].recorded_at) : "No data yet", readings[0] ? "Most recent saved sensor record" : "Waiting for ESP32 upload", "↗"),
          metric("Protected reserve", Number(farm?.tank_reserve_percent ?? 30) + "%", "Reserved from tank capacity", "⬡")
        ),
        h("section", { className: "panel" },
          h("div", { className: "panel-heading" }, h("div", null, h("h2", null, "Soil moisture by zone"), h("p", { className: "muted" }, "Latest readings saved by the controller")),
            h("span", { className: "small-muted" }, lastUpdated ? "Updated " + lastUpdated.toLocaleTimeString() : "Not updated")),
          zones.length ? h("div", { className: "zone-grid" }, ...zones.map(z => {
            const r = latestByZone.get(z.id);
            const moisture = r?.moisture_percent == null ? null : Number(r.moisture_percent);
            const threshold = Number(z.moisture_start_threshold ?? 40);
            const status = moisture == null ? "Waiting for data" : moisture < threshold ? "Needs attention" : "Moisture acceptable";
            return h("article", { className: "zone-card", key: z.id },
              h("div", { className: "zone-card-top" }, h("div", { className: "zone-icon" }, "♧"),
                h("span", { className: "soil-tag" }, (z.soil_type || "soil").toUpperCase())),
              h("h3", null, z.name),
              h("p", { className: "muted" }, "Start watering threshold: " + threshold + "%"),
              h("div", { className: "moisture-value" }, moisture == null ? "—" : moisture.toFixed(1) + "%"),
              h("div", { className: "progress-track" }, h("div", { className: "progress-fill " + (moisture != null && moisture < threshold ? "low" : ""), style: { width: (moisture == null ? 0 : Math.max(0, Math.min(100, moisture))) + "%" } })),
              h("p", { className: "zone-status" }, status),
              r ? h("p", { className: "small-muted" }, "Raw " + (r.raw_value ?? "—") + " · " + formatDate(r.recorded_at)) : h("p", { className: "small-muted" }, "No readings stored for this zone yet")
            );
          })) : h("div", { className: "empty-state" }, h("h3", null, "No zones configured"), h("p", null, "Set up the farm zones to start organising readings."), h("button", { className: "button primary", onClick: () => setPage("setup") }, "Set up farm")),
        ),
        h("section", { className: "panel" },
          h("div", { className: "panel-heading" }, h("div", null, h("h2", null, "Recent sensor readings"), h("p", { className: "muted" }, "Records returned from the Supabase database"))),
          readings.length ? h("div", { className: "table-wrap" }, h("table", null,
            h("thead", null, h("tr", null, h("th", null, "Recorded"), h("th", null, "Zone"), h("th", null, "Moisture"), h("th", null, "Raw value"), h("th", null, "Temp."), h("th", null, "Humidity"))),
            h("tbody", null, ...readings.slice(0, 15).map((r, i) => {
              const z = zones.find(x => x.id === r.zone_id);
              return h("tr", { key: r.id ?? i }, h("td", null, formatDate(r.recorded_at)), h("td", null, z?.name || "Unmapped zone"),
                h("td", null, r.moisture_percent == null ? "—" : Number(r.moisture_percent).toFixed(1) + "%"),
                h("td", null, r.raw_value ?? "—"), h("td", null, r.temperature_c == null ? "—" : Number(r.temperature_c).toFixed(1) + "°C"),
                h("td", null, r.humidity_percent == null ? "—" : Number(r.humidity_percent).toFixed(1) + "%"));
            }))
          )) : h("div", { className: "empty-state" }, h("p", null, device ? "The device is registered, but no sensor readings are available to this account yet." : "No controller is registered to this farm yet.")),
        ),
        h("section", { className: "panel" },
          h("div", { className: "panel-heading" },
            h("div", null, h("h2", null, "Irrigation history"), h("p", { className: "muted" }, "Recent irrigation cycles recorded by Agriguard")),
            h("span", { className: "small-muted" }, irrigationCycles.length + " recent cycle(s)")),
          irrigationCycles.length ? h("ol", { className: "timeline" }, ...irrigationCycles.map(cycle => {
            const zone = zones.find(z => z.id === cycle.zone_id);
            const status = (cycle.status || "unknown").replace(/_/g, " ");
            return h("li", { className: "timeline-item", key: cycle.id },
              h("span", { className: "timeline-marker " + (cycle.status === "completed" ? "completed" : cycle.status === "emergency_stopped" || cycle.status === "failed" ? "warning" : "") }),
              h("div", { className: "timeline-content" },
                h("div", { className: "timeline-title-row" },
                  h("h3", null, zone?.name || "Unmapped zone"),
                  h("span", { className: "cycle-status status-" + (cycle.status || "unknown") }, status)),
                h("p", { className: "small-muted" }, formatDate(cycle.started_at) + (cycle.ended_at ? " · Ended " + formatDate(cycle.ended_at) : " · In progress")),
                h("div", { className: "timeline-facts" },
                  h("span", null, "Duration: " + (cycle.duration_seconds == null ? "—" : formatDuration(cycle.duration_seconds))),
                  h("span", null, "Water: " + (cycle.water_used_l == null ? "—" : Number(cycle.water_used_l).toLocaleString(undefined, { maximumFractionDigits: 1 }) + " L")),
                  h("span", null, "Moisture: " + (cycle.moisture_before_percent == null ? "—" : Number(cycle.moisture_before_percent).toFixed(1) + "%") + " → " + (cycle.moisture_after_percent == null ? "—" : Number(cycle.moisture_after_percent).toFixed(1) + "%"))),
                cycle.trigger_reason ? h("p", { className: "timeline-reason" }, "Reason: " + cycle.trigger_reason) : null));
          })) : h("div", { className: "empty-state" },
            h("h3", null, "No irrigation history yet"),
            h("p", null, "Irrigation cycles will appear here once the ESP32 sends cycle records to Supabase. No sample history is shown as real data."))
        ),
        h("footer", { className: "dashboard-footer" }, "Data refreshes every 15 seconds while this page is open. Last refresh: " + (lastUpdated ? lastUpdated.toLocaleString() : "not yet") + ".")
      )
    );
  }

  if (!session) return authView();
  if (page === "setup" || !farm && page !== "dashboard") return setupView();
  return dashboardView();
}

function metric(label, value, detail, symbol) {
  return h("article", { className: "metric-card", key: label },
    h("div", { className: "metric-top" }, h("span", { className: "metric-label" }, label), h("span", { className: "metric-symbol" }, symbol)),
    h("div", { className: "metric-value" }, value),
    h("p", { className: "muted metric-detail" }, detail)
  );
}
function formatDuration(value) {
  const seconds = Math.max(0, Math.round(Number(value) || 0));
  if (seconds < 60) return seconds + "s";
  const minutes = Math.floor(seconds / 60);
  const remainder = seconds % 60;
  if (minutes < 60) return remainder ? minutes + "m " + remainder + "s" : minutes + "m";
  const hours = Math.floor(minutes / 60);
  const remainingMinutes = minutes % 60;
  return remainingMinutes ? hours + "h " + remainingMinutes + "m" : hours + "h";
}
function formatDate(value) {
  if (!value) return "—";
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? "—" : date.toLocaleString([], { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" });
}

createRoot(document.getElementById("root")).render(h(App));
