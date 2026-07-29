using UnityEngine;
using UnityEngine.InputSystem;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Smooth camera controller for ecosystem visualization.
    /// Uses New Input System. Supports ZQSD (AZERTY), WASD, and Arrow keys.
    /// Mouse scroll for zoom, right-click drag for rotation.
    /// </summary>
    public class EcosystemCameraController : MonoBehaviour
    {
        [Header("Movement")]
        [SerializeField] private float _moveSpeed = 30f;
        [SerializeField] private float _fastMoveMultiplier = 2.5f;
        [SerializeField] private float _smoothTime = 0.15f;
        
        [Header("Zoom")]
        [SerializeField] private float _zoomSpeed = 80f;
        [SerializeField] private float _minHeight = 10f;
        [SerializeField] private float _maxHeight = 150f;
        
        [Header("Rotation")]
        [SerializeField] private float _rotationSpeed = 100f;
        [SerializeField] private float _minPitch = 10f;
        [SerializeField] private float _maxPitch = 85f;
        
        [Header("Bounds")]
        [SerializeField] private float _maxDistance = 100f;
        
        private Vector3 _targetPosition;
        private Vector3 _velocity;
        private float _currentPitch;
        private float _currentYaw;
        
        private Keyboard _keyboard;
        private Mouse _mouse;
        
        private void Start()
        {
            _targetPosition = transform.position;
            _keyboard = Keyboard.current;
            _mouse = Mouse.current;
            
            // Extract current rotation
            Vector3 euler = transform.eulerAngles;
            _currentPitch = euler.x;
            _currentYaw = euler.y;
        }
        
        private void Update()
        {
            if (_keyboard == null) _keyboard = Keyboard.current;
            if (_mouse == null) _mouse = Mouse.current;
            
            if (_keyboard != null)
            {
                HandleMovement();
                HandleZoom();
                HandleRotation();
            }
            
            // Apply smooth movement
            transform.position = Vector3.SmoothDamp(transform.position, _targetPosition, ref _velocity, _smoothTime);
        }
        
        private void HandleMovement()
        {
            float speed = _moveSpeed;
            
            // Fast mode with Shift
            if (_keyboard.leftShiftKey.isPressed || _keyboard.rightShiftKey.isPressed)
            {
                speed *= _fastMoveMultiplier;
            }
            
            // Get input (supports ZQSD, WASD, and Arrows)
            float horizontal = 0f;
            float vertical = 0f;
            
            // Forward/Back: Z, W, Up Arrow
            if (_keyboard.zKey.isPressed || _keyboard.wKey.isPressed || _keyboard.upArrowKey.isPressed)
                vertical += 1f;
            if (_keyboard.sKey.isPressed || _keyboard.downArrowKey.isPressed)
                vertical -= 1f;
            
            // Left/Right: Q, A, Left Arrow
            if (_keyboard.qKey.isPressed || _keyboard.aKey.isPressed || _keyboard.leftArrowKey.isPressed)
                horizontal -= 1f;
            if (_keyboard.dKey.isPressed || _keyboard.rightArrowKey.isPressed)
                horizontal += 1f;
            
            if (horizontal != 0f || vertical != 0f)
            {
                // Get camera's forward and right vectors (flattened to XZ plane)
                Vector3 forward = transform.forward;
                forward.y = 0;
                forward.Normalize();
                
                Vector3 right = transform.right;
                right.y = 0;
                right.Normalize();
                
                // Calculate movement direction
                Vector3 movement = (forward * vertical + right * horizontal).normalized;
                _targetPosition += movement * speed * Time.deltaTime;
                
                // Clamp to bounds
                _targetPosition.x = Mathf.Clamp(_targetPosition.x, -_maxDistance, _maxDistance);
                _targetPosition.z = Mathf.Clamp(_targetPosition.z, -_maxDistance, _maxDistance);
            }
        }
        
        private void HandleZoom()
        {
            float scroll = 0f;
            
            if (_mouse != null)
            {
                scroll = _mouse.scroll.ReadValue().y * 0.01f;
            }
            
            // Also support +/- keys
            if (_keyboard.numpadPlusKey.isPressed || _keyboard.equalsKey.isPressed)
                scroll += 0.1f;
            if (_keyboard.numpadMinusKey.isPressed || _keyboard.minusKey.isPressed)
                scroll -= 0.1f;
            
            if (scroll != 0f)
            {
                // Zoom by moving forward/backward along view direction
                Vector3 zoomDirection = transform.forward;
                _targetPosition += zoomDirection * scroll * _zoomSpeed;
                
                // Clamp height
                _targetPosition.y = Mathf.Clamp(_targetPosition.y, _minHeight, _maxHeight);
            }
            
            // Page Up/Down or E/C for vertical movement
            if (_keyboard.pageUpKey.isPressed || _keyboard.eKey.isPressed)
                _targetPosition.y = Mathf.Min(_targetPosition.y + _zoomSpeed * Time.deltaTime, _maxHeight);
            if (_keyboard.pageDownKey.isPressed || _keyboard.cKey.isPressed)
                _targetPosition.y = Mathf.Max(_targetPosition.y - _zoomSpeed * Time.deltaTime, _minHeight);
        }
        
        private void HandleRotation()
        {
            if (_mouse == null) return;
            
            // Right-click drag to rotate
            if (_mouse.rightButton.isPressed)
            {
                Vector2 mouseDelta = _mouse.delta.ReadValue();
                float mouseX = mouseDelta.x * _rotationSpeed * 0.01f;
                float mouseY = mouseDelta.y * _rotationSpeed * 0.01f;
                
                _currentYaw += mouseX;
                _currentPitch -= mouseY;
                _currentPitch = Mathf.Clamp(_currentPitch, _minPitch, _maxPitch);
                
                transform.rotation = Quaternion.Euler(_currentPitch, _currentYaw, 0f);
            }
            
            // R/F for quick rotation (orbit)
            if (_keyboard.rKey.isPressed)
            {
                _currentYaw += _rotationSpeed * 0.5f * Time.deltaTime;
                transform.rotation = Quaternion.Euler(_currentPitch, _currentYaw, 0f);
            }
            if (_keyboard.fKey.isPressed)
            {
                _currentYaw -= _rotationSpeed * 0.5f * Time.deltaTime;
                transform.rotation = Quaternion.Euler(_currentPitch, _currentYaw, 0f);
            }
        }
        
        private void OnGUI()
        {
            // Show controls hint in corner
            GUIStyle style = new GUIStyle(GUI.skin.label);
            style.fontSize = 10;
            style.normal.textColor = new Color(1f, 1f, 1f, 0.5f);
            
            float y = Screen.height - 80;
            GUI.Label(new Rect(10, y, 300, 20), "ZQSD/Arrows: Move | Scroll: Zoom", style);
            GUI.Label(new Rect(10, y + 15, 300, 20), "Right-Click Drag: Rotate | Shift: Fast", style);
            GUI.Label(new Rect(10, y + 30, 300, 20), "E/C: Up/Down | R/F: Orbit", style);
        }
    }
}
