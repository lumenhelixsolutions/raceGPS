/**
 * Kinematic Arcade Car — no physics engine, just fun math.
 *
 * Coordinate system: X=east, Y=up, Z=south.
 * Heading: 0° = north (world -Z), clockwise.
 */
const METERS_PER_DEG_LAT = 111320;
function metersPerDegLon(lat) {
    return METERS_PER_DEG_LAT * Math.cos((lat * Math.PI) / 180);
}
export class ArcadeCar {
    x = 0;
    y = 0.7;
    z = 0;
    heading = 0; // degrees
    speed = 0; // m/s
    originLat = 0;
    originLon = 0;
    // Tunable arcade parameters
    maxSpeed = 35; // m/s ≈ 126 km/h
    reverseMax = 10; // m/s
    accel = 18; // m/s²
    reverseAccel = 8;
    brakeDecel = 30; // m/s²
    drag = 0.8; // per second
    offRoadDrag = 3.0; // extra drag when not on road
    maxSteerDeg = 40; // max wheel angle at standstill
    wheelbase = 2.8; // meters
    reset(lat, lon, headingDeg) {
        this.originLat = lat;
        this.originLon = lon;
        this.x = 0;
        this.z = 0;
        this.y = 0.7;
        this.heading = headingDeg;
        this.speed = 0;
    }
    update(inputs, dt) {
        // ── Throttle / Brake ──
        if (inputs.brake) {
            // Brake reduces speed toward zero
            const decel = this.brakeDecel * dt;
            if (this.speed > 0) {
                this.speed = Math.max(0, this.speed - decel);
            }
            else if (this.speed < 0) {
                this.speed = Math.min(0, this.speed + decel);
            }
        }
        else if (inputs.throttle > 0) {
            // Forward
            this.speed += inputs.throttle * this.accel * dt;
            this.speed = Math.min(this.speed, this.maxSpeed);
        }
        else if (inputs.throttle < 0) {
            // Reverse
            this.speed += inputs.throttle * this.reverseAccel * dt;
            this.speed = Math.max(this.speed, -this.reverseMax);
        }
        // ── Rolling drag ──
        const dragForce = this.drag * dt;
        if (this.speed > 0) {
            this.speed = Math.max(0, this.speed - dragForce * this.speed);
        }
        else if (this.speed < 0) {
            this.speed = Math.min(0, this.speed - dragForce * this.speed);
        }
        // ── Steering ──
        // Ackermann-ish: sharper steering at low speed for arcade feel
        const speedFactor = Math.min(Math.abs(this.speed) / 15, 1);
        const steerAngle = inputs.steer * this.maxSteerDeg * (1 - speedFactor * 0.4);
        // Turn rate: d(heading)/dt = speed * tan(steer) / wheelbase
        // For small angles: tan(θ) ≈ θ in radians
        const steerRad = (steerAngle * Math.PI) / 180;
        const turnRate = (this.speed * Math.tan(steerRad)) / this.wheelbase; // rad/s
        this.heading += (turnRate * dt * 180) / Math.PI;
        this.heading = ((this.heading % 360) + 360) % 360;
        // ── Position ──
        const headingRad = (this.heading * Math.PI) / 180;
        // Heading 0 = north = -Z
        this.x += Math.sin(headingRad) * this.speed * dt;
        this.z -= Math.cos(headingRad) * this.speed * dt;
        // ── Convert back to geo ──
        const dLat = -this.z / METERS_PER_DEG_LAT;
        const dLon = this.x / metersPerDegLon(this.originLat);
        const lat = this.originLat + dLat;
        const lon = this.originLon + dLon;
        return {
            lat,
            lon,
            heading: this.heading,
            speed: this.speed, // signed: positive = forward, negative = reverse
            x: this.x,
            y: this.y,
            z: this.z,
        };
    }
}
