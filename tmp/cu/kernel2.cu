struct Particle {
    long long x;
    long long y;
    long long vx;
    long long vy;
};

extern "C" __global__ void update_particles(
    Particle* particles, 
    int count, 
    long long boundsW, 
    long long boundsH,
    long long mouseX,
    long long mouseY,
    long long isAttracting
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    Particle p = particles[idx];

    if (isAttracting == 1) {
        // Calculăm direcția spre cursor
        long long dx = mouseX - p.x;
        long long dy = mouseY - p.y;
        
        // Accelerație spre mouse
        if (dx > 0) p.vx += 2; else if (dx < 0) p.vx -= 2;
        if (dy > 0) p.vy += 2; else if (dy < 0) p.vy -= 2;

        // Amortizare (frecare) pentru a stabiliza orbita în jurul cursorului
        p.vx = p.vx * 96 / 100;
        p.vy = p.vy * 96 / 100;
    } else {
        // Gravitație normală când nu se apasă click
        p.vy += 1;
    }

    // Actualizare poziție
    p.x += p.vx;
    p.y += p.vy;

    // Rebound podea și tavan
    if (p.y >= boundsH - 30) {
        p.y = boundsH - 30;
        p.vy = -p.vy * 7 / 10;
    } else if (p.y <= 10) {
        p.y = 10;
        p.vy = -p.vy;
    }

    // Rebound pereți
    if (p.x <= 20) {
        p.x = 20;
        p.vx = -p.vx;
    } else if (p.x >= boundsW - 20) {
        p.x = boundsW - 20;
        p.vx = -p.vx;
    }

    particles[idx] = p;
}